#pragma once

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>

namespace godot {

class BinaryMesherGDExtension : public RefCounted {
  GDCLASS(BinaryMesherGDExtension, RefCounted)

public:
  String get_status_message() const;
  Dictionary run_debug_test(const String &test_name) const;

protected:
  static void _bind_methods();
};

} // namespace godot
