/**
 * @file test_modal_framework.cpp
 * @brief Host-based unit tests for the ezmodes modal framework.
 *
 * Tests mode registration, lifecycle, transitions, inactivity timeout,
 * shutdown, and header bar configuration. Uses LVGL/FreeRTOS stubs.
 */

#include "ezmodes/modal_framework.hpp"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>

// ============================================================================
// Stub implementations (LVGL objects and FreeRTOS tick)
// ============================================================================

namespace {

/// Pool of fake LVGL objects for allocation tracking.
static constexpr int kMaxObjects = 64;
static lv_obj_t g_obj_pool[kMaxObjects];
static int g_obj_count = 0;

/// Top-layer singleton.
static lv_obj_t g_top_layer = {};

}  // namespace

lv_obj_t* lv_obj_create(lv_obj_t* parent) {
  assert(g_obj_count < kMaxObjects);
  lv_obj_t* obj = &g_obj_pool[g_obj_count++];
  *obj = {};
  obj->id = g_obj_count;
  obj->parent = parent;
  return obj;
}

lv_obj_t* lv_label_create(lv_obj_t* parent) {
  return lv_obj_create(parent);
}

lv_obj_t* lv_layer_top() {
  return &g_top_layer;
}

namespace freertos_stub {
static uint32_t g_tick_count = 0;
void set_tick_count(uint32_t ticks) { g_tick_count = ticks; }
uint32_t get_tick_count() { return g_tick_count; }
}  // namespace freertos_stub

/// Reset all stubs between tests.
static void reset_stubs() {
  g_obj_count = 0;
  g_top_layer = {};
  for (auto& obj : g_obj_pool) {
    obj = {};
  }
  freertos_stub::set_tick_count(0);
}

// ============================================================================
// Mock ModeInterface implementations
// ============================================================================

class MockMode : public ezmodes::ModeInterface {
 public:
  explicit MockMode(const char* id, const char* name = "Mock",
                    bool selectable = true)
      : id_(id), name_(name), selectable_(selectable) {}

  const char* get_id() const override { return id_; }
  const char* get_display_name() const override { return name_; }

  void on_enter(ezmodes::ModalFramework* fw, lv_obj_t* screen) override {
    enter_count++;
    last_framework = fw;
    last_screen = screen;
  }

  void on_exit() override {
    exit_count++;
  }

  void on_render(uint32_t delta_ms) override {
    render_count++;
    last_delta_ms = delta_ms;
  }

  void on_button_event(ezmodes::ButtonEvent event) override {
    button_count++;
    last_event = event;
  }

  bool is_selectable() const override { return selectable_; }

  // Tracking fields.
  int enter_count = 0;
  int exit_count = 0;
  int render_count = 0;
  int button_count = 0;
  ezmodes::ButtonEvent last_event = ezmodes::ButtonEvent::kKeyUp;
  uint32_t last_delta_ms = 0;
  ezmodes::ModalFramework* last_framework = nullptr;
  lv_obj_t* last_screen = nullptr;

 private:
  const char* id_;
  const char* name_;
  bool selectable_;
};

class TimeoutMode : public MockMode {
 public:
  TimeoutMode(const char* id, uint32_t timeout)
      : MockMode(id), timeout_(timeout) {}

  uint32_t get_inactivity_timeout_ms() const override { return timeout_; }

 private:
  uint32_t timeout_;
};

// ============================================================================
// Test infrastructure
// ============================================================================

namespace {

int tests_passed = 0;
int tests_failed = 0;

#define TEST_ASSERT(expr)                                                 \
  do {                                                                    \
    if (!(expr)) {                                                        \
      printf("FAIL: %s:%d: %s\n", __FILE__, __LINE__, #expr);            \
      tests_failed++;                                                     \
      return;                                                             \
    }                                                                     \
  } while (0)

#define RUN_TEST(test_func)                                               \
  do {                                                                    \
    reset_stubs();                                                        \
    printf("Running %s... ", #test_func);                                 \
    test_func();                                                          \
    printf("PASS\n");                                                     \
    tests_passed++;                                                       \
  } while (0)

// ============================================================================
// Tests: Mode Registration
// ============================================================================

void test_register_null_mode() {
  ezmodes::ModalFramework fw;
  TEST_ASSERT(!fw.register_mode(nullptr));
}

void test_register_valid_mode() {
  ezmodes::ModalFramework fw;
  auto* mode = new MockMode("test");
  TEST_ASSERT(fw.register_mode(mode));
  TEST_ASSERT(fw.find_mode("test") == mode);
}

void test_register_duplicate_id() {
  ezmodes::ModalFramework fw;
  fw.register_mode(new MockMode("dup"));
  TEST_ASSERT(!fw.register_mode(new MockMode("dup")));
}

void test_register_multiple_modes() {
  ezmodes::ModalFramework fw;
  fw.register_mode(new MockMode("a"));
  fw.register_mode(new MockMode("b"));
  fw.register_mode(new MockMode("c"));
  TEST_ASSERT(fw.find_mode("a") != nullptr);
  TEST_ASSERT(fw.find_mode("b") != nullptr);
  TEST_ASSERT(fw.find_mode("c") != nullptr);
}

// ============================================================================
// Tests: find_mode
// ============================================================================

void test_find_mode_not_found() {
  ezmodes::ModalFramework fw;
  TEST_ASSERT(fw.find_mode("nonexistent") == nullptr);
}

// ============================================================================
// Tests: set_initial_mode
// ============================================================================

void test_set_initial_mode_valid() {
  ezmodes::ModalFramework fw;
  fw.register_mode(new MockMode("init"));
  TEST_ASSERT(fw.set_initial_mode("init"));
  TEST_ASSERT(std::strcmp(fw.get_initial_mode_id(), "init") == 0);
}

void test_set_initial_mode_invalid() {
  ezmodes::ModalFramework fw;
  TEST_ASSERT(!fw.set_initial_mode("missing"));
  TEST_ASSERT(fw.get_initial_mode_id() == nullptr);
}

// ============================================================================
// Tests: start()
// ============================================================================

void test_start_without_initial_mode() {
  ezmodes::ModalFramework fw;
  fw.register_mode(new MockMode("m"));
  TEST_ASSERT(!fw.start());
}

void test_start_with_initial_mode() {
  ezmodes::ModalFramework fw;
  auto* mode = new MockMode("init");
  fw.register_mode(mode);
  fw.set_initial_mode("init");
  TEST_ASSERT(fw.start());
  TEST_ASSERT(fw.get_active_mode() == mode);
  TEST_ASSERT(mode->enter_count == 1);
  TEST_ASSERT(mode->last_framework == &fw);
  TEST_ASSERT(mode->last_screen != nullptr);
}

// ============================================================================
// Tests: get_active_mode
// ============================================================================

void test_active_mode_before_start() {
  ezmodes::ModalFramework fw;
  TEST_ASSERT(fw.get_active_mode() == nullptr);
}

// ============================================================================
// Tests: request_transition
// ============================================================================

void test_request_transition_valid() {
  ezmodes::ModalFramework fw;
  auto* a = new MockMode("a");
  auto* b = new MockMode("b");
  fw.register_mode(a);
  fw.register_mode(b);
  fw.set_initial_mode("a");
  fw.start();

  TEST_ASSERT(fw.request_transition("b"));
  // Transition is deferred — a is still active.
  TEST_ASSERT(fw.get_active_mode() == a);
}

void test_request_transition_invalid() {
  ezmodes::ModalFramework fw;
  fw.register_mode(new MockMode("a"));
  fw.set_initial_mode("a");
  fw.start();
  TEST_ASSERT(!fw.request_transition("nonexistent"));
}

// ============================================================================
// Tests: tick() transition
// ============================================================================

void test_tick_executes_transition() {
  ezmodes::ModalFramework fw;
  auto* a = new MockMode("a", "Mode A");
  auto* b = new MockMode("b", "Mode B");
  fw.register_mode(a);
  fw.register_mode(b);
  fw.set_initial_mode("a");
  fw.start();

  fw.request_transition("b");
  freertos_stub::set_tick_count(100);
  fw.tick();

  TEST_ASSERT(fw.get_active_mode() == b);
  TEST_ASSERT(a->exit_count == 1);
  TEST_ASSERT(b->enter_count == 1);
  TEST_ASSERT(b->last_screen != nullptr);
}

void test_tick_calls_render() {
  ezmodes::ModalFramework fw;
  auto* mode = new MockMode("m");
  fw.register_mode(mode);
  fw.set_initial_mode("m");
  fw.start();

  freertos_stub::set_tick_count(50);
  fw.tick();
  freertos_stub::set_tick_count(80);
  fw.tick();

  TEST_ASSERT(mode->render_count >= 2);
}

// ============================================================================
// Tests: handle_button_event
// ============================================================================

void test_button_event_routed_to_mode() {
  ezmodes::ModalFramework fw;
  auto* mode = new MockMode("m");
  fw.register_mode(mode);
  fw.set_initial_mode("m");
  fw.start();

  fw.handle_button_event(ezmodes::ButtonEvent::kShortPress);
  TEST_ASSERT(mode->button_count == 1);
  TEST_ASSERT(mode->last_event == ezmodes::ButtonEvent::kShortPress);

  fw.handle_button_event(ezmodes::ButtonEvent::kLongPress);
  TEST_ASSERT(mode->button_count == 2);
  TEST_ASSERT(mode->last_event == ezmodes::ButtonEvent::kLongPress);
}

// ============================================================================
// Tests: get_selectable_modes
// ============================================================================

void test_selectable_modes() {
  ezmodes::ModalFramework fw;
  fw.register_mode(new MockMode("sel1", "Sel1", true));
  fw.register_mode(new MockMode("hidden", "Hidden", false));
  fw.register_mode(new MockMode("sel2", "Sel2", true));

  auto selectable = fw.get_selectable_modes();
  TEST_ASSERT(selectable.size() == 2);
}

// ============================================================================
// Tests: Inactivity timeout
// ============================================================================

void test_inactivity_timeout_default() {
  ezmodes::ModalFramework fw;
  TEST_ASSERT(fw.get_inactivity_timeout() ==
              ezmodes::ModalFramework::kDefaultInactivityTimeoutMs);
}

void test_inactivity_timeout_set_get() {
  ezmodes::ModalFramework fw;
  fw.set_inactivity_timeout(60000);
  TEST_ASSERT(fw.get_inactivity_timeout() == 60000);
}

void test_inactivity_triggers_power_off() {
  ezmodes::ModalFramework fw;
  auto* main_mode = new MockMode("main");
  auto* standby = new MockMode("standby");
  fw.register_mode(main_mode);
  fw.register_mode(standby);
  fw.set_initial_mode("main");
  fw.set_inactivity_timeout(1000);  // 1 second
  fw.set_power_off_mode("standby");

  freertos_stub::set_tick_count(0);
  fw.start();

  // Simulate time passing beyond timeout.
  freertos_stub::set_tick_count(1500);
  fw.tick();  // This should request transition.

  // Transition is pending; need another tick.
  freertos_stub::set_tick_count(1501);
  fw.tick();

  TEST_ASSERT(fw.get_active_mode() == standby);
}

void test_mode_custom_timeout() {
  // Mode with custom timeout of 0 (disabled) should not trigger.
  ezmodes::ModalFramework fw;
  auto* mode = new TimeoutMode("patience", 0);
  auto* standby = new MockMode("standby");
  fw.register_mode(mode);
  fw.register_mode(standby);
  fw.set_initial_mode("patience");
  fw.set_inactivity_timeout(500);
  fw.set_power_off_mode("standby");

  freertos_stub::set_tick_count(0);
  fw.start();

  // Well past the global timeout.
  freertos_stub::set_tick_count(5000);
  fw.tick();

  // Should still be in patience mode because it disabled timeout.
  TEST_ASSERT(fw.get_active_mode() == mode);
}

// ============================================================================
// Tests: Shutdown
// ============================================================================

static bool g_shutdown_called = false;
static void mock_shutdown() {
  g_shutdown_called = true;
}

void test_request_shutdown_null_callback() {
  ezmodes::ModalFramework fw;
  fw.register_mode(new MockMode("m"));
  fw.set_initial_mode("m");
  fw.start();
  // Should not crash.
  fw.request_shutdown(nullptr);
}

void test_request_shutdown_executes_on_tick() {
  g_shutdown_called = false;
  ezmodes::ModalFramework fw;
  auto* mode = new MockMode("m");
  fw.register_mode(mode);
  fw.set_initial_mode("m");
  fw.start();

  fw.request_shutdown(mock_shutdown);
  freertos_stub::set_tick_count(10);
  fw.tick();

  TEST_ASSERT(g_shutdown_called);
  TEST_ASSERT(mode->exit_count == 1);
}

// ============================================================================
// Tests: Header bar configuration
// ============================================================================

void test_content_height_no_header() {
  ezmodes::ModalFramework fw;
  // Default display is 128 vertical (from stub).
  int32_t h = fw.get_content_height();
  TEST_ASSERT(h == 128);
}

void test_content_height_with_header() {
  ezmodes::ModalFramework fw;
  ezmodes::HeaderBarConfig hdr = {12, nullptr};
  fw.set_header_config(hdr);
  int32_t h = fw.get_content_height();
  TEST_ASSERT(h == 128 - 12);
}

void test_header_widget_area_no_header() {
  ezmodes::ModalFramework fw;
  TEST_ASSERT(fw.get_header_widget_area() == nullptr);
}

// ============================================================================
// Tests: ModeInterface defaults
// ============================================================================

void test_mode_interface_default_selectable() {
  // MockMode defaults to selectable=true, matching base class.
  MockMode m("x");
  TEST_ASSERT(m.is_selectable());
}

void test_mode_interface_default_timeout() {
  MockMode m("x");
  TEST_ASSERT(m.get_inactivity_timeout_ms() ==
              ezmodes::ModeInterface::kUseDefaultTimeout);
}

// ============================================================================
// Tests: ButtonEvent enum
// ============================================================================

void test_button_event_values_distinct() {
  TEST_ASSERT(ezmodes::ButtonEvent::kShortPress !=
              ezmodes::ButtonEvent::kLongPress);
  TEST_ASSERT(ezmodes::ButtonEvent::kLongPress !=
              ezmodes::ButtonEvent::kKeyUp);
  TEST_ASSERT(ezmodes::ButtonEvent::kShortPress !=
              ezmodes::ButtonEvent::kKeyUp);
}

// ============================================================================
// Tests: Destructor / lifecycle
// ============================================================================

void test_destructor_deletes_modes() {
  // We can't directly verify deletion, but we can verify no crash.
  auto* fw = new ezmodes::ModalFramework();
  fw->register_mode(new MockMode("a"));
  fw->register_mode(new MockMode("b"));
  delete fw;  // Should delete both modes without crash.
}

}  // namespace

// ============================================================================
// Main
// ============================================================================

int main() {
  printf("=== ezmodes-esp-base Host Tests ===\n\n");

  // Registration.
  RUN_TEST(test_register_null_mode);
  RUN_TEST(test_register_valid_mode);
  RUN_TEST(test_register_duplicate_id);
  RUN_TEST(test_register_multiple_modes);
  RUN_TEST(test_find_mode_not_found);

  // Initial mode & start.
  RUN_TEST(test_set_initial_mode_valid);
  RUN_TEST(test_set_initial_mode_invalid);
  RUN_TEST(test_start_without_initial_mode);
  RUN_TEST(test_start_with_initial_mode);
  RUN_TEST(test_active_mode_before_start);

  // Transitions.
  RUN_TEST(test_request_transition_valid);
  RUN_TEST(test_request_transition_invalid);
  RUN_TEST(test_tick_executes_transition);
  RUN_TEST(test_tick_calls_render);

  // Button events.
  RUN_TEST(test_button_event_routed_to_mode);

  // Selectable modes.
  RUN_TEST(test_selectable_modes);

  // Inactivity timeout.
  RUN_TEST(test_inactivity_timeout_default);
  RUN_TEST(test_inactivity_timeout_set_get);
  RUN_TEST(test_inactivity_triggers_power_off);
  RUN_TEST(test_mode_custom_timeout);

  // Shutdown.
  RUN_TEST(test_request_shutdown_null_callback);
  RUN_TEST(test_request_shutdown_executes_on_tick);

  // Header bar.
  RUN_TEST(test_content_height_no_header);
  RUN_TEST(test_content_height_with_header);
  RUN_TEST(test_header_widget_area_no_header);

  // Interface defaults.
  RUN_TEST(test_mode_interface_default_selectable);
  RUN_TEST(test_mode_interface_default_timeout);
  RUN_TEST(test_button_event_values_distinct);
  RUN_TEST(test_destructor_deletes_modes);

  printf("\n=== Results ===\n");
  printf("Passed: %d, Failed: %d\n", tests_passed, tests_failed);

  return tests_failed > 0 ? 1 : 0;
}
