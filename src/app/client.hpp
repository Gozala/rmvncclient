#ifndef APP_CLIENT_HPP
#define APP_CLIENT_HPP

#include "event_loop.hpp"
#include "screen.hpp"
#include "touch.hpp"
#include <array>
#include <iosfwd>
#include <poll.h> // IWYU pragma: keep
#include <rfb/rfbclient.h>
// IWYU pragma: no_include "rfb/rfbproto.h"

namespace rmioc
{
    class screen;
    class touch;
}

namespace app
{

/**
 * VNC client for the reMarkable tablet.
 */
class client
{
public:
    /**
     * Create a VNC client.
     *
     * @param ip IP address of the VNC server to connect to.
     * @param port Port of the VNC server to connect to.
     * @param screen_device Screen to update with data from the VNC server.
     * @param touch_device Touchscreen device to read input events from.
     */
    client(
        const char* ip, int port,
        rmioc::screen& screen_device,
        rmioc::touch& touch_device
    );

    ~client();

    /**
     * Start the client event loop.
     */
    void event_loop();

private:
    /** List of file descriptors to watch in the event loop. */
    std::array<pollfd, 2> polled_fds;

    static constexpr std::size_t poll_vnc = 0;
    static constexpr std::size_t poll_touch = 1;

    /** Subroutine for handling VNC events from the server. */
    event_loop_status event_loop_vnc();

    /** VNC connection. */
    rfbClient* vnc_client;

    /** Event handler for the screen device. */
    screen screen_handler;

    /** Event handler for the touch device. */
    touch touch_handler;

    /**
     * Send press and release events for the given button to VNC.
     *
     * @param x Pointer X location on the screen.
     * @param y Pointer Y location on the screen.
     * @param button Button to press.
     */
    void send_button_press(int x, int y, MouseButton button);
}; // class client

} // namespace app

#endif // APP_CLIENT_HPP
