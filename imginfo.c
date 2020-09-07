/* quick program to inspect a FX3 firmware image and dump its contents
 * Franco Venturi - Sun 06 Sep 2020 02:57:54 PM EDT
 */

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>

static int process_program_header(int fd, char *prefix);

int main(int argc, char **argv)
{
  if (argc < 2) {
    fprintf(stderr, "usage: %s [-d] <image file>\n", argv[0]);
  }
  int dump = strcmp(argv[1], "-d") == 0;
  char *imagefile = dump ? argv[2] : argv[1];
  int fd = open(imagefile, O_RDONLY);
  if (fd < 0) {
    fprintf(stderr, "open(%s) failed: %s\n", imagefile, strerror(errno));
    return 1;
  }

  /* image header */
  unsigned char image_header[4];
  ssize_t nread = read(fd, image_header, sizeof(image_header));
  if (nread != sizeof(image_header)) {
    fprintf(stderr, "error reading image header\n");
    close(fd);
    return 1;
  }
  if (!(image_header[0] == 'C' && image_header[1] == 'Y')) {
    fprintf(stderr, "image header doesn't start with 'CY'\n");
    close(fd);
    return 1;
  }
  fprintf(stdout, "i2cConf=%02x\n", image_header[2]);
  fprintf(stdout, "imgType=%02x\n", image_header[3]);

  char *prefix = 0;
  if (dump) {
    prefix = imagefile;
    *(strrchr(prefix, '.')+1) = '\0';
  }

  int elfHdr_phnum = 0;
  while (process_program_header(fd, argc > 2 ? argv[2] : 0)) {
    elfHdr_phnum++;
  }
  //fprintf(stdout, "elfHdr.phnum=%d\n", elfHdr_phnum);

  uint32_t entryAddr;
  read(fd, &entryAddr, sizeof(entryAddr));
  fprintf(stdout, "Entry point=0x%08x\n", entryAddr);
  uint32_t checksum;
  read(fd, &checksum, sizeof(checksum));
  //fprintf(stdout, "checksum=%08x\n", checksum);

  close(fd);
  return 0;
}

static int process_program_header(int fd, char *prefix)
{
  uint32_t loadSz;
  read(fd, &loadSz, sizeof(loadSz));
  if (loadSz == 0) {
    return 0;
  }
  uint32_t secStart;
  read(fd, &secStart, sizeof(secStart));
  fprintf(stdout, "\tAddr=0x%08x Size(words)=0x%08x [%d]\n", secStart,
          loadSz, loadSz);
  if (prefix == 0) {
    lseek(fd, 4 * loadSz, SEEK_CUR);
  } else {
    char *suffix = "unknown";
    int append = 0;
    if (secStart >= 0x40030000) {
      suffix = "data";
      append = secStart > 0x40030000;
    } else if (secStart >= 0x40003000) {
      suffix = "code";
      append = secStart > 0x40003000;
    } else if (secStart >= 0x00000100) {
      suffix = "itcm";
      append = secStart > 0x00000100;
    }
    char filename[256];
    snprintf(filename, sizeof(filename), "%s%s", prefix, suffix);
    int fd1 = open(filename, O_WRONLY | (append ? O_APPEND : (O_CREAT | O_TRUNC)),
                   0644);
    if (fd1 < 0) {
      fprintf(stderr, "open(%s) failed: %s\n", filename, strerror(errno));
      lseek(fd, 4 * loadSz, SEEK_CUR);
    } else {
      char buffer[8192];
      for (ssize_t i = 4 * loadSz; i > 0; ) {
        ssize_t nr = read(fd, buffer, i < sizeof(buffer) ? i : sizeof(buffer));
        write(fd1, buffer, nr);
        i -= nr;
      }
      close(fd1);
    }
  }
  return loadSz;
}
