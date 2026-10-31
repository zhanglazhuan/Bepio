#include "tts_text.h"
#include <stddef.h>

bool bepio_tts_text_valid(const char *text)
{
	if (!text) { return false; }
	size_t length = 0;
	while (length <= BEPIO_TTS_TEXT_MAX && text[length]) { length++; }
	if (!length || length > BEPIO_TTS_TEXT_MAX) { return false; }
	bool content = false;
	for (size_t i = 0; i < length;) {
		unsigned char c = (unsigned char)text[i];
		if (c < 0x80) {
			if (c == 0x7f || (c < 0x20 && c != '\n' && c != '\t')) { return false; }
			content |= c != ' ' && c != '\n' && c != '\t';
			i++;
			continue;
		}
		size_t width = c >= 0xc2 && c <= 0xdf ? 2 :
			c >= 0xe0 && c <= 0xef ? 3 : c >= 0xf0 && c <= 0xf4 ? 4 : 0;
		if (!width || i + width > length) { return false; }
		for (size_t j = 1; j < width; j++) {
			unsigned char next = (unsigned char)text[i + j];
			if (next < 0x80 || next > 0xbf) { return false; }
		}
		unsigned char second = (unsigned char)text[i + 1];
		if ((c == 0xe0 && second < 0xa0) || (c == 0xed && second > 0x9f) ||
		    (c == 0xf0 && second < 0x90) || (c == 0xf4 && second > 0x8f)) {
			return false;
		}
		content = true;
		i += width;
	}
	return content;
}
