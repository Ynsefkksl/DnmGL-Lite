#include "DnmGLLite/DnmGLLite.hpp"

#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
    #define NOMINMAX
#endif
#include <windows.h>
#include <libloaderapi.h>

namespace DnmGLLite::Windows {
    class ContextLoader : DnmGLLite::ContextLoader {
    public:
        ContextLoader(std::string_view shared_path) {
            //TODO: choose dx and vulkan api 
            shared = LoadLibrary(shared_path.data());
            if (!shared) {
                //throw DnmGLLite::Error("failed to context lib load");
            }

            const auto load_func = (DnmGLLite::Context*((*)()))GetProcAddress(shared, "LoadContext");
            if (!load_func) {
                FreeLibrary(shared);
                //throw DnmGLLite::Error("failed to context load function");
            }
            
            context = load_func();
        }

        ~ContextLoader() {
            if (context) {
                delete context;
                FreeLibrary(shared);
            }
        }

        DnmGLLite::Context *GetContext() const { return context; }

        ContextLoader(ContextLoader&&) = delete;
        ContextLoader(ContextLoader&) = delete;
        ContextLoader& operator=(ContextLoader&&) = delete;
        ContextLoader& operator=(ContextLoader&) = delete;
    private:
        DnmGLLite::Context *context = nullptr;
        HMODULE shared = nullptr;
    };
};