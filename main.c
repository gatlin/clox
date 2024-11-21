#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "chunk.h"
#include "debug.h"
#include "value.h"
#include "vm.h"

static void
repl() {
  char line[1024];
  for (;;) {
    printf("> ");
    if (!fgets(line, sizeof(line), stdin)) {
      printf("\n");
      break;
    }
    interpret(line);
  }
}

static char*
readFile (const char *path) {
  char *buffer;
  size_t fileSize, bytesRead;
  FILE *file = fopen (path, "rb");
  if (NULL == file) {
    fprintf (stderr, "Could not open file \"%s\".\n", path);
    exit (74);
  }
  fseek (file, 0L, SEEK_END);
  fileSize = ftell (file);
  rewind (file);
  buffer = (char *)malloc (fileSize + 1);
  if (NULL == buffer) {
    fprintf (stderr, "Not enough memory to read \"%s\".\n", path);
    exit (74);
  }
  bytesRead = fread (buffer, sizeof (char), fileSize, file);
  if (bytesRead < fileSize) {
    fprintf (stderr, "Could not read file \"%s\".\n", path);
    exit (74);
  }
  buffer[bytesRead] = '\0';
  fclose (file);
  return buffer;
}

static void
runFile (const char *path) {
  char *source = readFile(path);
  InterpretResult result = interpret (source);
  free (source);
  if (INTERPRET_COMPILE_ERROR == result) { exit(65); }
  if (INTERPRET_RUNTIME_ERROR == result) { exit(70); }
}

int
main (int argc, const char **argv) {
  initVM ();
  if (1 == argc) {
    repl();
  }
  else if (2 == argc) {
    runFile(argv[1]);
  }
  else {
    fprintf(stderr, "Usage: clox [path]\n");
    exit(64);
  }

  freeVM ();
  return 0;
}
