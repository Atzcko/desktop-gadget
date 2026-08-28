#include "theme.h"
#include "settings.h"

static const Theme THEMES[] = {
    { "Fliqlo",
      0x161616, 0x161616, 0xFFFFFF, 0x707070,
      0x161616, 0x161616,
      { 0x161616, 0x161616, 0x161616 },
      0x2A2A2A, 0x161616, 0 },
    { "Pop",              /* the bento board: vivid cards, black ground */
      0x3D8BE8, 0xE8442C, 0xFFFFFF, 0xF5D22D,
      0xF08A28, 0x3D8BE8,
      { 0xF08A28, 0x3D8BE8, 0xE8442C },
      0xF08A28, 0xE8442C, 6 },
};

int theme_count(void) { return (int)(sizeof(THEMES) / sizeof(THEMES[0])); }

const Theme &theme_at(int i)
{
    if (i < 0 || i >= theme_count()) i = 0;
    return THEMES[i];
}

const Theme &theme_get(void) { return theme_at(settings_get().theme); }
