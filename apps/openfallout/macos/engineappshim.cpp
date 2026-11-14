// Executable of OpenFallout.app/Contents/Helpers/OpenFallout Engine.app.
//
// The Qt launcher is the main executable of OpenFallout.app, so `open` on that bundle starts the launcher. The nested
// engine app gives `open` (and so Game Mode) a bundle whose program is the engine. This shim replaces itself with
// Contents/MacOS/openfallout of the outer app, keeping the process, its arguments and its environment. After the exec,
// the engine's executable path is the outer one, so @executable_path library references, the plug-in folders and the
// Resources lookup work exactly as when the engine is started directly; the large binary is not duplicated.

#include <mach-o/dyld.h>

#include <climits>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unistd.h>
#include <vector>

int main(int argc, char* argv[])
{
    (void)argc;
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::vector<char> buffer(size);
    char resolved[PATH_MAX];
    if (_NSGetExecutablePath(buffer.data(), &size) != 0 || realpath(buffer.data(), resolved) == nullptr)
    {
        std::perror("OpenFallout Engine: cannot find its own path");
        return 127;
    }

    // <outer>/Contents/Helpers/OpenFallout Engine.app/Contents/MacOS/OpenFallout Engine -> <outer>/Contents
    std::string path = resolved;
    for (int i = 0; i < 5; ++i)
    {
        const std::string::size_type slash = path.rfind('/');
        if (slash == std::string::npos || slash == 0)
        {
            std::fprintf(stderr, "OpenFallout Engine: not inside OpenFallout.app/Contents/Helpers: %s\n", resolved);
            return 127;
        }
        path.erase(slash);
    }
    path += "/MacOS/openfallout";

    argv[0] = path.data();
    execv(path.c_str(), argv);
    std::perror(("OpenFallout Engine: cannot start " + path).c_str());
    return 127;
}
