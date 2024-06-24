#ifndef	md5_h
#define	md5_h

void	md5_buffer_b64(const uint8_t *buf, int buflen, char *str);
void md5_buffer( const uint8_t *buf, int buflen, uint8_t digest[16]);
char *md5_digest_string(uint8_t d[16], char digest_string[33]);

#endif
