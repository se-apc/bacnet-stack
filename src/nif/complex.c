/* complex.c */

// This is the library of functions

static int stat_var = 5;

int foo(int x) {
  stat_var += x;
  return x+1;
}

int bar(int y) {
  stat_var-= y;
  return y*2;
}


int get_var() {
  return stat_var;
}