#include "defs.h"
#include "refresh.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <rfb/rfbclient.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>

#define FRAMEBUF_FP_TAG 0

/**
 * Initialize the client framebuffer, a buffer in which screen updates from
 * the server are stored.
 *
 * @return True if the new buffer was successfully allocated.
 */
rfbBool create_framebuf(rfbClient* client)
{
    uint8_t* data = malloc(client->width * client->height * RM_SCREEN_DEPTH);

    // Make sure the server complied with our pixel depth request
    assert(client->format.bitsPerPixel == RM_SCREEN_DEPTH * 8);

    if (!data)
    {
        perror("create_framebuf");
        return FALSE;
    }

    free(client->frameBuffer);
    client->frameBuffer = data;
    return TRUE;
}

/**
 * Flush an update from the client framebuffer to the device framebuffer.
 *
 * @param x Left bound of the updated rectangle (in pixels).
 * @param y Top bound of the updated rectangle (in pixels).
 * @param w Width of the updated rectangle (in pixels).
 * @param h Height of the updated rectangle (in pixels).
 */
void update_framebuf(rfbClient* client, int x, int y, int w, int h)
{
    // Ignore off-screen updates
    if (x > RM_SCREEN_COLS || y > RM_SCREEN_ROWS)
    {
        return;
    }

    // Clip overflowing updates
    if (x + w > RM_SCREEN_COLS)
    {
        w = RM_SCREEN_COLS - x;
    }

    if (y + h > RM_SCREEN_ROWS)
    {
        h = RM_SCREEN_ROWS - y;
    }

    // Seek to the first pixel in the device framebuffer
    int framebuf_fp = *(int*) rfbClientGetClientData(client, FRAMEBUF_FP_TAG);
    off_t new_position = lseek(
        framebuf_fp,
        (y * (RM_SCREEN_COLS + RM_SCREEN_COL_PAD) + x) * RM_SCREEN_DEPTH,
        SEEK_SET
    );

    if (new_position == -1)
    {
        perror("update_framebuf");
        exit(EXIT_FAILURE);
    }

    // Seek to the first pixel in the client framebuffer
    uint8_t* framebuf_client = client->frameBuffer
        + (y * client->width + x) * RM_SCREEN_DEPTH;

    for (int line = 0; line < h; ++line)
    {
        size_t line_size = w * RM_SCREEN_DEPTH;

        while (line_size)
        {
            ssize_t bytes_written = write(
                framebuf_fp,
                framebuf_client,
                w * RM_SCREEN_DEPTH
            );

            if (bytes_written == -1)
            {
                perror("update_framebuf");
                exit(EXIT_FAILURE);
            }

            line_size -= bytes_written;
            framebuf_client += bytes_written;
        }

        new_position = lseek(
            framebuf_fp,
            (RM_SCREEN_COLS + RM_SCREEN_COL_PAD - w) * RM_SCREEN_DEPTH,
            SEEK_CUR
        );

        if (new_position == -1)
        {
            perror("update_framebuf");
            exit(EXIT_FAILURE);
        }

        framebuf_client += (client->width - w) * RM_SCREEN_DEPTH;
    }

    // TODO: Only refresh updated zone
    trigger_refresh(framebuf_fp, 0, 0, RM_SCREEN_COLS, RM_SCREEN_ROWS);
}

int main(int argc, char** argv)
{
    rfbClient* client = rfbGetClient(
        /* bitsPerSample = */ 16,
        /* samplesPerPixel = */ 1,
        /* bytesPerPixel = */ 2
    );

    client->MallocFrameBuffer = create_framebuf;
    client->GotFrameBufferUpdate = update_framebuf;

    if (!rfbInitClient(client, &argc, argv))
    {
        return EXIT_FAILURE;
    }

    // Open system framebuffer
    int framebuf_fp = open("/dev/fb0", O_WRONLY);

    if (framebuf_fp == -1)
    {
        perror("open /dev/fb0");
        return EXIT_FAILURE;
    }

    rfbClientSetClientData(client, FRAMEBUF_FP_TAG, &framebuf_fp);

    // RFB protocol message loop
    while (TRUE)
    {
        int result = WaitForMessage(client, /* timeout μs = */ 1000000);

        if (result < 0)
        {
            perror("WaitForMessage");
            break;
        }

        if (result > 0)
        {
            if (!HandleRFBServerMessage(client))
            {
                fprintf(stderr, "Unable to handle message\n");
                break;
            }
        }
    }

    rfbClientCleanup(client);
    close(framebuf_fp);
    return EXIT_SUCCESS;
}
