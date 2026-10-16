#include "tests.hpp"
#include "helpers.hpp"

#include "../../hyprctlCompat.hpp"
#include "../../Log.hpp"
#include "../shared.hpp"
#include "../clients/build.hpp"

#include <hyprutils/os/FileDescriptor.hpp>
#include <hyprutils/os/Process.hpp>

#include <chrono>
#include <csignal>
#include <format>
#include <thread>
#include <unistd.h>

using namespace Hyprutils::OS;
using namespace Hyprutils::Memory;
using namespace StackTest;

namespace {
    // Drives the child-window test client: a parent toplevel ("child-test-parent") which opens
    // a child toplevel with xdg_toplevel.set_parent ("child-test-child") on request.
    class CParentChildClient {
      public:
        bool start() {
            m_proc = makeUnique<CProcess>(std::format("{}/child-window", binaryDir), std::vector<std::string>{});
            m_proc->addEnv("WAYLAND_DISPLAY", WLDISPLAY);

            int pipeFds[2];
            if (pipe(pipeFds) != 0)
                return false;

            m_writeFd = CFileDescriptor(pipeFds[1]);
            m_proc->setStdinFD(pipeFds[0]);

            if (!m_proc->runAsync())
                return false;
            close(pipeFds[0]);

            return waitForWindows(1);
        }

        bool openChild() {
            const std::string CMD = "toplevel\n";
            if (sc<size_t>(write(m_writeFd.get(), CMD.c_str(), CMD.length())) != CMD.length())
                return false;
            return waitForWindows(2);
        }

        ~CParentChildClient() {
            if (!m_proc)
                return;
            const std::string CMD = "exit\n";
            write(m_writeFd.get(), CMD.c_str(), CMD.length());
            kill(m_proc->pid(), SIGKILL);
        }

      private:
        bool waitForWindows(int n) {
            for (int i = 0; i < 100 && Tests::windowCount() < n; ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            Tests::sync();
            return Tests::windowCount() >= n;
        }

        CUniquePointer<CProcess> m_proc;
        CFileDescriptor          m_writeFd;
    };
}

TEST_CASE(stackTransientStaysAboveParent) {
    enterStacking();

    {
        CParentChildClient client;
        if (!client.start())
            FAIL_TEST("Couldn't start the child-window client");
        if (!client.openChild())
            FAIL_TEST("Child window didn't open");

        EXPECT(window("child-test-child").floating, true);
        EXPECT(isAbove("child-test-child", "child-test-parent"), true);

        // clicking / focusing the parent must not bury its dialog
        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:child-test-parent' })"));
        OK(getFromSocket("/dispatch hl.dsp.window.alter_zorder({ mode = 'top', window = 'class:child-test-parent' })"));
        Tests::sync();
        EXPECT(isAbove("child-test-child", "child-test-parent"), true);

        // transients are centered over their parent
        const auto PARENT = window("child-test-parent").box;
        const auto CHILD  = window("child-test-child").box;
        EXPECT_MAX_DELTA(CHILD.x + CHILD.w / 2, PARENT.x + PARENT.w / 2, 2);
        EXPECT_MAX_DELTA(CHILD.y + CHILD.h / 2, PARENT.y + PARENT.h / 2, 2);
    }

    leaveStacking();
}
