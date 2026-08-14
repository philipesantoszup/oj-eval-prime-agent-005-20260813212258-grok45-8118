#ifndef QOI_FORMAT_CODEC_QOI_H_
#define QOI_FORMAT_CODEC_QOI_H_

#include "utils.h"

constexpr uint8_t QOI_OP_INDEX_TAG = 0x00;
constexpr uint8_t QOI_OP_DIFF_TAG  = 0x40;
constexpr uint8_t QOI_OP_LUMA_TAG  = 0x80;
constexpr uint8_t QOI_OP_RUN_TAG   = 0xc0; 
constexpr uint8_t QOI_OP_RGB_TAG   = 0xfe;
constexpr uint8_t QOI_OP_RGBA_TAG  = 0xff;
constexpr uint8_t QOI_PADDING[8] = {0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u};
constexpr uint8_t QOI_MASK_2 = 0xc0;

/**
 * @brief encode the raw pixel data of an image to qoi format.
 *
 * @param[in] width image width in pixels
 * @param[in] height image height in pixels
 * @param[in] channels number of color channels, 3 = RGB, 4 = RGBA
 * @param[in] colorspace image color space, 0 = sRGB with linear alpha, 1 = all channels linear
 *
 * @return bool true if it is a valid qoi format image, false otherwise
 */
bool QoiEncode(uint32_t width, uint32_t height, uint8_t channels, uint8_t colorspace = 0);

/**
 * @brief decode the qoi format of an image to raw pixel data
 *
 * @param[out] width image width in pixels
 * @param[out] height image height in pixels
 * @param[out] channels number of color channels, 3 = RGB, 4 = RGBA
 * @param[out] colorspace image color space, 0 = sRGB with linear alpha, 1 = all channels linear
 *
 * @return bool true if it is a valid qoi format image, false otherwise
 */
bool QoiDecode(uint32_t &width, uint32_t &height, uint8_t &channels, uint8_t &colorspace);


bool QoiEncode(uint32_t width, uint32_t height, uint8_t channels, uint8_t colorspace) {
    if (width == 0 || height == 0 || (channels != 3 && channels != 4) || colorspace > 1) {
        return false;
    }

    // qoi-header part
    QoiWriteChar('q');
    QoiWriteChar('o');
    QoiWriteChar('i');
    QoiWriteChar('f');
    QoiWriteU32(width);
    QoiWriteU32(height);
    QoiWriteU8(channels);
    QoiWriteU8(colorspace);

    /* qoi-data part */
    int run = 0;
    int px_num = static_cast<int>(width * height);

    uint8_t history[64][4];
    memset(history, 0, sizeof(history));

    uint8_t r = 0u, g = 0u, b = 0u, a = 255u;
    uint8_t pre_r = 0u, pre_g = 0u, pre_b = 0u, pre_a = 255u;

    for (int i = 0; i < px_num; ++i) {
        r = QoiReadU8();
        g = QoiReadU8();
        b = QoiReadU8();
        if (channels == 4) a = QoiReadU8();

        if (r == pre_r && g == pre_g && b == pre_b && a == pre_a) {
            // QOI_OP_RUN
            run++;
            if (run == 62 || i == px_num - 1) {
                QoiWriteU8(QOI_OP_RUN_TAG | static_cast<uint8_t>(run - 1));
                run = 0;
            }
        } else {
            if (run > 0) {
                QoiWriteU8(QOI_OP_RUN_TAG | static_cast<uint8_t>(run - 1));
                run = 0;
            }

            int index_pos = QoiColorHash(r, g, b, a);

            if (history[index_pos][0] == r &&
                history[index_pos][1] == g &&
                history[index_pos][2] == b &&
                history[index_pos][3] == a) {
                // QOI_OP_INDEX
                QoiWriteU8(QOI_OP_INDEX_TAG | static_cast<uint8_t>(index_pos));
            } else {
                // update history
                history[index_pos][0] = r;
                history[index_pos][1] = g;
                history[index_pos][2] = b;
                history[index_pos][3] = a;

                if (a == pre_a) {
                    // differences with wraparound using int8
                    int8_t vr = static_cast<int8_t>(r - pre_r);
                    int8_t vg = static_cast<int8_t>(g - pre_g);
                    int8_t vb = static_cast<int8_t>(b - pre_b);

                    int8_t vg_r = static_cast<int8_t>(vr - vg);
                    int8_t vg_b = static_cast<int8_t>(vb - vg);

                    if (vr > -3 && vr < 2 && vg > -3 && vg < 2 && vb > -3 && vb < 2) {
                        // QOI_OP_DIFF
                        QoiWriteU8(static_cast<uint8_t>(
                            QOI_OP_DIFF_TAG |
                            ((vr + 2) << 4) |
                            ((vg + 2) << 2) |
                            (vb + 2)
                        ));
                    } else if (vg_r > -9 && vg_r < 8 && vg > -33 && vg < 32 && vg_b > -9 && vg_b < 8) {
                        // QOI_OP_LUMA
                        QoiWriteU8(static_cast<uint8_t>(QOI_OP_LUMA_TAG | (vg + 32)));
                        QoiWriteU8(static_cast<uint8_t>(((vg_r + 8) << 4) | (vg_b + 8)));
                    } else {
                        // QOI_OP_RGB
                        QoiWriteU8(QOI_OP_RGB_TAG);
                        QoiWriteU8(r);
                        QoiWriteU8(g);
                        QoiWriteU8(b);
                    }
                } else {
                    // QOI_OP_RGBA
                    QoiWriteU8(QOI_OP_RGBA_TAG);
                    QoiWriteU8(r);
                    QoiWriteU8(g);
                    QoiWriteU8(b);
                    QoiWriteU8(a);
                }
            }
        }

        pre_r = r;
        pre_g = g;
        pre_b = b;
        pre_a = a;
    }

    // qoi-padding part
    for (int i = 0; i < static_cast<int>(sizeof(QOI_PADDING) / sizeof(QOI_PADDING[0])); ++i) {
        QoiWriteU8(QOI_PADDING[i]);
    }

    return true;
}

bool QoiDecode(uint32_t &width, uint32_t &height, uint8_t &channels, uint8_t &colorspace) {
    char c1 = QoiReadChar();
    char c2 = QoiReadChar();
    char c3 = QoiReadChar();
    char c4 = QoiReadChar();
    if (c1 != 'q' || c2 != 'o' || c3 != 'i' || c4 != 'f') {
        return false;
    }

    width = QoiReadU32();
    height = QoiReadU32();
    channels = QoiReadU8();
    colorspace = QoiReadU8();

    if (width == 0 || height == 0 || (channels != 3 && channels != 4) || colorspace > 1) {
        return false;
    }

    int run = 0;
    int px_num = static_cast<int>(width * height);

    uint8_t history[64][4];
    memset(history, 0, sizeof(history));

    uint8_t r = 0u, g = 0u, b = 0u, a = 255u;

    for (int i = 0; i < px_num; ++i) {
        if (run > 0) {
            run--;
        } else {
            uint8_t tag = QoiReadU8();

            if (tag == QOI_OP_RGB_TAG) {
                r = QoiReadU8();
                g = QoiReadU8();
                b = QoiReadU8();
            } else if (tag == QOI_OP_RGBA_TAG) {
                r = QoiReadU8();
                g = QoiReadU8();
                b = QoiReadU8();
                a = QoiReadU8();
            } else if ((tag & QOI_MASK_2) == QOI_OP_INDEX_TAG) {
                int index = tag & 0x3f;
                r = history[index][0];
                g = history[index][1];
                b = history[index][2];
                a = history[index][3];
            } else if ((tag & QOI_MASK_2) == QOI_OP_DIFF_TAG) {
                r += static_cast<uint8_t>(((tag >> 4) & 0x03) - 2);
                g += static_cast<uint8_t>(((tag >> 2) & 0x03) - 2);
                b += static_cast<uint8_t>(( tag       & 0x03) - 2);
            } else if ((tag & QOI_MASK_2) == QOI_OP_LUMA_TAG) {
                uint8_t b2 = QoiReadU8();
                int vg = static_cast<int>(tag & 0x3f) - 32;
                r += static_cast<uint8_t>(vg - 8 + ((b2 >> 4) & 0x0f));
                g += static_cast<uint8_t>(vg);
                b += static_cast<uint8_t>(vg - 8 + (b2 & 0x0f));
            } else if ((tag & QOI_MASK_2) == QOI_OP_RUN_TAG) {
                run = tag & 0x3f;
            }

            int index_pos = QoiColorHash(r, g, b, a);
            history[index_pos][0] = r;
            history[index_pos][1] = g;
            history[index_pos][2] = b;
            history[index_pos][3] = a;
        }

        QoiWriteU8(r);
        QoiWriteU8(g);
        QoiWriteU8(b);
        if (channels == 4) QoiWriteU8(a);
    }

    bool valid = true;
    for (int i = 0; i < static_cast<int>(sizeof(QOI_PADDING) / sizeof(QOI_PADDING[0])); ++i) {
        if (QoiReadU8() != QOI_PADDING[i]) valid = false;
    }

    return valid;
}

#endif // QOI_FORMAT_CODEC_QOI_H_
