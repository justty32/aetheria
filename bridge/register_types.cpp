#include "bridge/register_types.h"

#include "bridge/aetheria_core.h"

#include <godot_cpp/godot.hpp>

void initialize_aetheria_module(godot::ModuleInitializationLevel level) {
    if (level != godot::MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }
    GDREGISTER_CLASS(aetheria::bridge::AetheriaCore);
}

void uninitialize_aetheria_module(godot::ModuleInitializationLevel level) {
    if (level != godot::MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }
}

extern "C" {

GDExtensionBool GDE_EXPORT aetheria_library_init(
    GDExtensionInterfaceGetProcAddress get_proc_address,
    GDExtensionClassLibraryPtr library,
    GDExtensionInitialization* initialization) {
    godot::GDExtensionBinding::InitObject init_object(get_proc_address, library, initialization);
    init_object.register_initializer(initialize_aetheria_module);
    init_object.register_terminator(uninitialize_aetheria_module);
    init_object.set_minimum_library_initialization_level(
        godot::MODULE_INITIALIZATION_LEVEL_SCENE);
    return init_object.init();
}

}  // extern "C"

