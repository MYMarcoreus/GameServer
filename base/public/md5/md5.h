#pragma once

namespace md5 {
// md5 encrypt function
void EncryptMD5(unsigned char *output, unsigned char *input, int len);

void EncryptMD5str(char *output, unsigned char *input, int len);
}
