// tool for testing
#include <errno.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char *argv[]) {
  if (argc != 3) {
    fprintf(stderr, "usage: %s input-file output-file\n", argv[0]);
    return 1;
  }

  const char *input_filename = argv[1];
  const char *output_filename = argv[2];

  FILE *input_file = fopen(input_filename, "r");
  if (!input_file) {
    fprintf(stderr, "failed to open `%s' for reading: %s\n", input_filename,
            strerror(errno));
    return 1;
  }

  FILE *output_file = fopen(output_filename, "w");
  if (!output_file) {
    fclose(input_file);
    fprintf(stderr, "failed to open `%s' for writing: %s\n", output_filename,
            strerror(errno));
    return 1;
  }

  // count bytes
  fseek(input_file, 0L, SEEK_END);
  size_t bytes = ftell(input_file);
  rewind(input_file);
  fprintf(output_file, "Content-Length: %zu\r\n", bytes);

  // write the input file contents
  char buffer[BUFSIZ];
  for (size_t read_amt = 0;
       (read_amt = fread(buffer, 1, sizeof buffer, input_file));)
    fwrite(buffer, 1, read_amt, output_file);

  fclose(input_file);
  fclose(output_file);
  return 0;
}
