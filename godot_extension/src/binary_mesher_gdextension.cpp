#include "binary_mesher_gdextension.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

void BinaryMesherGDExtension::_bind_methods() {
  ClassDB::bind_method(D_METHOD("get_status_message"), &BinaryMesherGDExtension::get_status_message);
  ClassDB::bind_method(D_METHOD("run_debug_test", "test_name"), &BinaryMesherGDExtension::run_debug_test);
}

String BinaryMesherGDExtension::get_status_message() const {
  return "[b]GDExtension Ready[/b]\nThis is a minimal Godot C++ bridge for menu-driven debug tests.";
}

Dictionary BinaryMesherGDExtension::run_debug_test(const String &test_name) const {
  Dictionary result;
  result["test"] = test_name;
  result["passed"] = true;

  if (test_name == "memory") {
    result["details"] = "Memory test simulated: alloc/free cycle completed.";
  } else if (test_name == "mesher") {
    result["details"] = "Mesher smoke test simulated: basic path returned expected values.";
  } else if (test_name == "ui") {
    result["details"] = "UI debug test simulated: main menu callbacks fired.";
  } else {
    result["passed"] = false;
    result["details"] = "Unknown test key. Supported keys: memory, mesher, ui.";
  }

  UtilityFunctions::print(String("[BinaryMesherGDExtension] Test '") + test_name + "' completed.");
  return result;
}

} // namespace godot
