// The animated examples of what each piece of code does. See piece_demos.h.
//
// Each example is short (a few lines, a few seconds) and shows one idea with values the
// learner can follow. Frames say what happens after each line runs; the test runs every
// program, so a frame that doesn't match what the code really does fails the build's tests.
#include "level/piece_demos.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const PieceDemo demos[] = {
    // ---- A first program ----
    { "#include <stdio.h> main printf", false,
      "#include <stdio.h>\n"
      "int main(void) {\n"
      "    printf(\"Hello\\n\");\n"
      "    printf(\"World\\n\");\n"
      "    return 0;\n"
      "}",
      { { 1, "" }, { 2, "" }, { 3, "", "Hello\n" }, { 4, "", "World\n" }, { 5, "" } } },

    { "return", false,
      "int twice(int n) {\n"
      "    return n * 2;\n"
      "}\n"
      "int main(void) {\n"
      "    int x = twice(4);\n"
      "    printf(\"%d\\n\", x);\n"
      "    return 0;\n"
      "}",
      { { 4, "" },
        { 5, "x=?" },
        { 1, "x=?;n=4" },
        { 2, "x=?;n=4", .eval = "4 * 2", .result = "8" },
        { 5, "x=8" },
        { 6, "x=8", "8\n", .look = "x" },
        { 7, "x=8" } } },

    { "int", false,
      "int apples = 3;\n"
      "int pears = 4;\n"
      "int fruit = apples + pears;\n"
      "printf(\"%d\\n\", fruit);",
      { { 1, "apples=3" },
        { 2, "apples=3;pears=4" },
        { 3, "apples=3;pears=4;fruit=7", .eval = "3 + 4", .result = "7" },
        { 4, "apples=3;pears=4;fruit=7", "7\n", .look = "fruit" } } },

    { "\\n", false,
      "printf(\"A\");\n"
      "printf(\"B\\n\");\n"
      "printf(\"C\\n\");",
      { { 1, "", "A" }, { 2, "", "B\n" }, { 3, "", "C\n" } } },

    { "\\\"", false,
      "printf(\"say \\\"hi\\\"\\n\");\n"
      "printf(\"\\\"ok\\\"\\n\");",
      { { 1, "", "say \"hi\"\n" }, { 2, "", "\"ok\"\n" } } },

    // ---- Values and printing them ----
    { "%d", false,
      "int age = 20;\n"
      "printf(\"I am %d\\n\", age);\n"
      "age = 21;\n"
      "printf(\"I am %d\\n\", age);",
      { { 1, "age=20" },
        { 2, "age=20", "I am 20\n", .look = "age" },
        { 3, "age=21" },
        { 4, "age=21", "I am 21\n", .look = "age" } } },

    { "double", false,
      "double half = 7 / 2.0;\n"
      "int whole = 7 / 2;\n"
      "printf(\"%f\\n\", half);\n"
      "printf(\"%d\\n\", whole);",
      { { 1, "half=3.5", .eval = "7 / 2.0", .result = "3.5" },
        { 2, "half=3.5;whole=3", .eval = "7 / 2", .result = "3" },
        { 3, "half=3.5;whole=3", "3.500000\n", .look = "half" },
        { 4, "half=3.5;whole=3", "3\n", .look = "whole" } } },

    { "%.2f", false,
      "double pi = 3.14159;\n"
      "printf(\"%.2f\\n\", pi);\n"
      "printf(\"%.4f\\n\", pi);",
      { { 1, "pi=3.14159" },
        { 2, "pi=3.14159", "3.14\n", .look = "pi" },
        { 3, "pi=3.14159", "3.1416\n", .look = "pi" } } },

    { "%.1f", false,
      "double t = 21.47;\n"
      "printf(\"%.1f\\n\", t);\n"
      "printf(\"%.3f\\n\", t);",
      { { 1, "t=21.47" }, { 2, "t=21.47", "21.5\n", .look = "t" }, { 3, "t=21.47", "21.470\n", .look = "t" } } },

    { "char", false,
      "char grade = 'B';\n"
      "printf(\"%c\\n\", grade);\n"
      "grade = 'A';\n"
      "printf(\"%c\\n\", grade);",
      { { 1, "grade='B'" },
        { 2, "grade='B'", "B\n", .look = "grade" },
        { 3, "grade='A'" },
        { 4, "grade='A'", "A\n", .look = "grade" } } },

    { "%c", false,
      "char a = 'H';\n"
      "char b = 'i';\n"
      "printf(\"%c%c!\\n\", a, b);",
      { { 1, "a='H'" }, { 2, "a='H';b='i'" }, { 3, "a='H';b='i'", "Hi!\n" } } },

    { "%f", false,
      "float w = 2.5;\n"
      "printf(\"%f\\n\", w);\n"
      "w = w * 2;\n"
      "printf(\"%f\\n\", w);",
      { { 1, "w=2.5" },
        { 2, "w=2.5", "2.500000\n", .look = "w" },
        { 3, "w=5", .eval = "2.5 * 2", .result = "5" },
        { 4, "w=5", "5.000000\n", .look = "w" } } },

    { "const", false,
      "const int days = 7;\n"
      "int weeks = 3;\n"
      "printf(\"%d\\n\", days * weeks);",
      { { 1, "!days=7" }, { 2, "!days=7;weeks=3" }, { 3, "!days=7;weeks=3", "21\n", .eval = "7 * 3", .result = "21" } } },

    { "float", false,
      "float h = 1.5;\n"
      "h = h + 0.25;\n"
      "printf(\"%.2f\\n\", h);",
      { { 1, "h=1.5" }, { 2, "h=1.75", .eval = "1.5 + 0.25", .result = "1.75" }, { 3, "h=1.75", "1.75\n", .look = "h" } } },

    // ---- Input ----
    { "scanf &", false,
      "int n;\n"
      "scanf(\"%d\", &n);\n"
      "printf(\"%d\\n\", n * 2);",
      { { 1, "n=?" },
        { 2, "n=6", .typed = "6\n", .look = "n" },
        { 3, "n=6", "12\n", .eval = "6 * 2", .result = "12" } } },

    { "%lf", false,
      "double d;\n"
      "scanf(\"%lf\", &d);\n"
      "printf(\"%.1f\\n\", d * 2);",
      { { 1, "d=?" },
        { 2, "d=1.25", .typed = "1.25\n", .look = "d" },
        { 3, "d=1.25", "2.5\n", .eval = "1.25 * 2", .result = "2.5" } } },

    { "%49s", false,
      "char word[50];\n"
      "scanf(\"%49s\", word);\n"
      "printf(\"%s!\\n\", word);",
      { { 1, "word=[?,?,?,?,?,?,...]" },
        { 2, "word=[h,e,l,l,o,\\0,...]", .typed = "hello world\n", .look = "word" },
        { 3, "word=[h,e,l,l,o,\\0,...]", "hello!\n", .look = "word" } } },

    // ---- Arithmetic ----
    { "%", false,
      "int a = 17;\n"
      "int b = 5;\n"
      "printf(\"%d\\n\", a / b);\n"
      "printf(\"%d\\n\", a % b);",
      { { 1, "a=17" },
        { 2, "a=17;b=5" },
        { 3, "a=17;b=5", "3\n", .eval = "17 / 5", .result = "3" },
        { 4, "a=17;b=5", "2\n", .eval = "17 % 5", .result = "2" } } },

    { "++", false,
      "int n = 5;\n"
      "n++;\n"
      "n++;\n"
      "printf(\"%d\\n\", n);",
      { { 1, "n=5" }, { 2, "n=6" }, { 3, "n=7" }, { 4, "n=7", "7\n", .look = "n" } } },

    { "--", false,
      "int lives = 3;\n"
      "lives--;\n"
      "lives--;\n"
      "printf(\"%d\\n\", lives);",
      { { 1, "lives=3" }, { 2, "lives=2" }, { 3, "lives=1" }, { 4, "lives=1", "1\n", .look = "lives" } } },

    { "+=", false,
      "int total = 10;\n"
      "total += 5;\n"
      "total += 5;\n"
      "printf(\"%d\\n\", total);",
      { { 1, "total=10" },
        { 2, "total=15", .eval = "10 + 5", .result = "15" },
        { 3, "total=20", .eval = "15 + 5", .result = "20" },
        { 4, "total=20", "20\n", .look = "total" } } },

    { "-=", false,
      "int hp = 10;\n"
      "hp -= 3;\n"
      "hp -= 3;\n"
      "printf(\"%d\\n\", hp);",
      { { 1, "hp=10" },
        { 2, "hp=7", .eval = "10 - 3", .result = "7" },
        { 3, "hp=4", .eval = "7 - 3", .result = "4" },
        { 4, "hp=4", "4\n", .look = "hp" } } },

    { "*=", false,
      "int x = 2;\n"
      "x *= 3;\n"
      "x *= 3;\n"
      "printf(\"%d\\n\", x);",
      { { 1, "x=2" },
        { 2, "x=6", .eval = "2 * 3", .result = "6" },
        { 3, "x=18", .eval = "6 * 3", .result = "18" },
        { 4, "x=18", "18\n", .look = "x" } } },

    { "/=", false,
      "int x = 100;\n"
      "x /= 2;\n"
      "x /= 5;\n"
      "printf(\"%d\\n\", x);",
      { { 1, "x=100" },
        { 2, "x=50", .eval = "100 / 2", .result = "50" },
        { 3, "x=10", .eval = "50 / 5", .result = "10" },
        { 4, "x=10", "10\n", .look = "x" } } },

    { "%3d", false,
      "printf(\"[%3d]\\n\", 7);\n"
      "printf(\"[%3d]\\n\", 42);\n"
      "printf(\"[%3d]\\n\", 123);",
      { { 1, "", "[  7]\n" }, { 2, "", "[ 42]\n" }, { 3, "", "[123]\n" } } },

    // ---- Comparisons: 1 is true, 0 is false ----
    { ">=", false,
      "int a = 5;\n"
      "printf(\"%d\\n\", a >= 5);\n"
      "printf(\"%d\\n\", a >= 6);",
      { { 1, "a=5" }, { 2, "a=5", "1\n", .eval = "5 >= 5", .result = "1" }, { 3, "a=5", "0\n", .eval = "5 >= 6", .result = "0" } } },

    { "<=", false,
      "int a = 5;\n"
      "printf(\"%d\\n\", a <= 5);\n"
      "printf(\"%d\\n\", a <= 4);",
      { { 1, "a=5" }, { 2, "a=5", "1\n", .eval = "5 <= 5", .result = "1" }, { 3, "a=5", "0\n", .eval = "5 <= 4", .result = "0" } } },

    { ">", false,
      "int a = 5;\n"
      "printf(\"%d\\n\", a > 4);\n"
      "printf(\"%d\\n\", a > 5);",
      { { 1, "a=5" }, { 2, "a=5", "1\n", .eval = "5 > 4", .result = "1" }, { 3, "a=5", "0\n", .eval = "5 > 5", .result = "0" } } },

    { "<", false,
      "int a = 5;\n"
      "printf(\"%d\\n\", a < 9);\n"
      "printf(\"%d\\n\", a < 5);",
      { { 1, "a=5" }, { 2, "a=5", "1\n", .eval = "5 < 9", .result = "1" }, { 3, "a=5", "0\n", .eval = "5 < 5", .result = "0" } } },

    { "==", false,
      "int a = 5;\n"
      "printf(\"%d\\n\", a == 5);\n"
      "printf(\"%d\\n\", a == 6);",
      { { 1, "a=5" }, { 2, "a=5", "1\n", .eval = "5 == 5", .result = "1" }, { 3, "a=5", "0\n", .eval = "5 == 6", .result = "0" } } },

    { "!=", false,
      "int a = 5;\n"
      "printf(\"%d\\n\", a != 6);\n"
      "printf(\"%d\\n\", a != 5);",
      { { 1, "a=5" }, { 2, "a=5", "1\n", .eval = "5 != 6", .result = "1" }, { 3, "a=5", "0\n", .eval = "5 != 5", .result = "0" } } },

    // ---- Decisions ----
    { "if", false,
      "int temp = 30;\n"
      "if (temp >= 25)\n"
      "    printf(\"hot\\n\");\n"
      "temp = 10;\n"
      "if (temp >= 25)\n"
      "    printf(\"hot\\n\");\n"
      "printf(\"end\\n\");",
      { { 1, "temp=30" },
        { 2, "temp=30", .eval = "30 >= 25", .result = "1" },
        { 3, "temp=30", "hot\n" },
        { 4, "temp=10" },
        { 5, "temp=10", .eval = "10 >= 25", .result = "0" },
        { 7, "temp=10", "end\n" } } },

    { "else", false,
      "int score = 40;\n"
      "if (score >= 50)\n"
      "    printf(\"pass\\n\");\n"
      "else\n"
      "    printf(\"fail\\n\");",
      { { 1, "score=40" }, { 2, "score=40", .eval = "40 >= 50", .result = "0" }, { 4, "score=40" }, { 5, "score=40", "fail\n" } } },

    { "||", false,
      "int day = 7;\n"
      "if (day == 6 || day == 7)\n"
      "    printf(\"weekend\\n\");",
      { { 1, "day=7" },
        { 2, "day=7", .eval = "7 == 6", .result = "0" },
        { 2, "day=7", .eval = "7 == 7", .result = "1" },
        { 2, "day=7", .eval = "0 || 1", .result = "1" },
        { 3, "day=7", "weekend\n" } } },

    { "&&", false,
      "int t = 30;\n"
      "if (t >= 15 && t <= 25)\n"
      "    printf(\"nice\\n\");\n"
      "printf(\"end\\n\");",
      { { 1, "t=30" },
        { 2, "t=30", .eval = "30 >= 15", .result = "1" },
        { 2, "t=30", .eval = "30 <= 25", .result = "0" },
        { 2, "t=30", .eval = "1 && 0", .result = "0" },
        { 4, "t=30", "end\n" } } },

    { "switch case break", false,
      "int day = 2;\n"
      "switch (day) {\n"
      "case 1: printf(\"Mon\\n\"); break;\n"
      "case 2: printf(\"Tue\\n\"); break;\n"
      "case 3: printf(\"Wed\\n\"); break;\n"
      "}\n"
      "printf(\"end\\n\");",
      { { 1, "day=2" }, { 2, "day=2", .eval = "day", .result = "2" }, { 4, "day=2", "Tue\n" }, { 7, "day=2", "end\n" } } },

    { "default", false,
      "int day = 9;\n"
      "switch (day) {\n"
      "case 1: printf(\"Mon\\n\"); break;\n"
      "case 2: printf(\"Tue\\n\"); break;\n"
      "default: printf(\"?\\n\");\n"
      "}",
      { { 1, "day=9" }, { 2, "day=9", .eval = "day", .result = "9" }, { 5, "day=9", "?\n" } } },

    { "?", false,
      "int a = 4;\n"
      "int b = 9;\n"
      "int big = a > b ? a : b;\n"
      "printf(\"%d\\n\", big);",
      { { 1, "a=4" },
        { 2, "a=4;b=9" },
        { 3, "a=4;b=9", .eval = "4 > 9", .result = "0" },
        { 3, "a=4;b=9;big=9", .eval = "0 ? 4 : 9", .result = "9" },
        { 4, "a=4;b=9;big=9", "9\n", .look = "big" } } },

    { "<stdbool.h> bool true false", false,
      "#include <stdbool.h>\n"
      "#include <stdio.h>\n"
      "int main(void) {\n"
      "    bool done = false;\n"
      "    done = true;\n"
      "    printf(\"%d\\n\", done);\n"
      "    return 0;\n"
      "}",
      { { 3, "" }, { 4, "done=false" }, { 5, "done=true" }, { 6, "done=true", "1\n", .look = "done" }, { 7, "done=true" } } },

    // ---- Loops ----
    { "while", false,
      "int n = 3;\n"
      "while (n > 0) {\n"
      "    printf(\"%d\\n\", n);\n"
      "    n = n - 1;\n"
      "}\n"
      "printf(\"go\\n\");",
      { { 1, "n=3" },
        { 2, "n=3", .eval = "3 > 0", .result = "1" },
        { 3, "n=3", "3\n" },
        { 4, "n=2" },
        { 2, "n=2", .eval = "2 > 0", .result = "1" },
        { 3, "n=2", "2\n" },
        { 4, "n=1" },
        { 2, "n=1", .eval = "1 > 0", .result = "1" },
        { 3, "n=1", "1\n" },
        { 4, "n=0" },
        { 2, "n=0", .eval = "0 > 0", .result = "0" },
        { 6, "n=0", "go\n" } } },

    { "for", false,
      "for (int i = 0; i < 3; i++)\n"
      "    printf(\"%d \", i);\n"
      "printf(\"\\n\");",
      { { 1, "i=0", .eval = "0 < 3", .result = "1" },
        { 2, "i=0", "0 " },
        { 1, "i=1", .eval = "1 < 3", .result = "1" },
        { 2, "i=1", "1 " },
        { 1, "i=2", .eval = "2 < 3", .result = "1" },
        { 2, "i=2", "2 " },
        { 1, "i=3", .eval = "3 < 3", .result = "0" },
        { 3, "", "\n" } } },

    { "do", false,
      "int n = 10;\n"
      "do {\n"
      "    printf(\"%d\\n\", n);\n"
      "    n++;\n"
      "} while (n < 3);",
      { { 1, "n=10" }, { 2, "n=10" }, { 3, "n=10", "10\n" }, { 4, "n=11" }, { 5, "n=11", .eval = "11 < 3", .result = "0" } } },

    { "continue", false,
      "for (int i = 1; i <= 3; i++) {\n"
      "    if (i == 2)\n"
      "        continue;\n"
      "    printf(\"%d\\n\", i);\n"
      "}",
      { { 1, "i=1", .eval = "1 <= 3", .result = "1" },
        { 2, "i=1", .eval = "1 == 2", .result = "0" },
        { 4, "i=1", "1\n" },
        { 1, "i=2", .eval = "2 <= 3", .result = "1" },
        { 2, "i=2", .eval = "2 == 2", .result = "1" },
        { 3, "i=2" },
        { 1, "i=3", .eval = "3 <= 3", .result = "1" },
        { 2, "i=3", .eval = "3 == 2", .result = "0" },
        { 4, "i=3", "3\n" },
        { 1, "i=4", .eval = "4 <= 3", .result = "0" } } },

    // ---- Arrays and strings ----
    { "[]", false,
      "int a[3] = {4, 8, 15};\n"
      "printf(\"%d\\n\", a[0]);\n"
      "a[2] = 16;\n"
      "printf(\"%d\\n\", a[2]);",
      { { 1, "a=[4,8,15]" },
        { 2, "a=[4,8,15]", "4\n", .look = "a[0]" },
        { 3, "a=[4,8,16]", .look = "a[2]" },
        { 4, "a=[4,8,16]", "16\n", .look = "a[2]" } } },

    { "sizeof", false,
      "int a[5] = {1, 2, 3, 4, 5};\n"
      "int n = sizeof(a) / sizeof(a[0]);\n"
      "printf(\"%d\\n\", n);",
      { { 1, "a=[1,2,3,4,5]" },
        { 2, "a=[1,2,3,4,5]", .eval = "sizeof(a)", .result = "20", .look = "a" },
        { 2, "a=[1,2,3,4,5]", .eval = "sizeof(a[0])", .result = "4", .look = "a[0]" },
        { 2, "a=[1,2,3,4,5];n=5", .eval = "20 / 4", .result = "5" },
        { 3, "a=[1,2,3,4,5];n=5", "5\n", .look = "n" } } },

    { "%s", false,
      "char name[] = \"Bob\";\n"
      "printf(\"Hi %s\\n\", name);\n"
      "name[0] = 'R';\n"
      "printf(\"Hi %s\\n\", name);",
      { { 1, "name=[B,o,b,\\0]" },
        { 2, "name=[B,o,b,\\0]", "Hi Bob\n", .look = "name" },
        { 3, "name=[R,o,b,\\0]", .look = "name[0]" },
        { 4, "name=[R,o,b,\\0]", "Hi Rob\n", .look = "name" } } },

    { "\\0", false,
      "char s[6] = \"Hello\";\n"
      "printf(\"%s\\n\", s);\n"
      "s[2] = '\\0';\n"
      "printf(\"%s\\n\", s);",
      { { 1, "s=[H,e,l,l,o,\\0]" },
        { 2, "s=[H,e,l,l,o,\\0]", "Hello\n", .look = "s" },
        { 3, "s=[H,e,\\0,l,o,\\0]", .look = "s[2]" },
        { 4, "s=[H,e,\\0,l,o,\\0]", "He\n", .look = "s" } } },

    // ---- Functions ----
    { "void", false,
      "void hello(void) {\n"
      "    printf(\"hi\\n\");\n"
      "}\n"
      "int main(void) {\n"
      "    hello();\n"
      "    hello();\n"
      "    return 0;\n"
      "}",
      { { 4, "" }, { 5, "" }, { 2, "", "hi\n" }, { 3, "" }, { 6, "" }, { 2, "", "hi\n" }, { 3, "" }, { 7, "" } } },

    // ---- Pointers ----
    { "int* *", false,
      "int x = 5;\n"
      "int *p = &x;\n"
      "*p = 9;\n"
      "printf(\"%d\\n\", x);",
      { { 1, "x=5" }, { 2, "x=5;p=&x" }, { 3, "x=9;p=&x", .look = "x" }, { 4, "x=9;p=&x", "9\n", .look = "x" } } },

    { "NULL", false,
      "int *p = NULL;\n"
      "int x = 3;\n"
      "p = &x;\n"
      "printf(\"%d\\n\", *p);",
      { { 1, "p=NULL" }, { 2, "p=NULL;x=3" }, { 3, "p=&x;x=3" }, { 4, "p=&x;x=3", "3\n", .look = "x" } } },

    { "char*", false,
      "char *s = \"cat\";\n"
      "printf(\"%c\\n\", *s);\n"
      "s++;\n"
      "printf(\"%c\\n\", *s);",
      { { 1, "s=&#1[0];#1=[c,a,t,\\0]" },
        { 2, "s=&#1[0];#1=[c,a,t,\\0]", "c\n", .look = "#1[0]" },
        { 3, "s=&#1[1];#1=[c,a,t,\\0]" },
        { 4, "s=&#1[1];#1=[c,a,t,\\0]", "a\n", .look = "#1[1]" } } },

    { "int**", false,
      "int x = 1;\n"
      "int *p = &x;\n"
      "int **pp = &p;\n"
      "**pp = 7;\n"
      "printf(\"%d\\n\", x);",
      { { 1, "x=1" },
        { 2, "x=1;p=&x" },
        { 3, "x=1;p=&x;pp=&p" },
        { 4, "x=7;p=&x;pp=&p", .look = "x" },
        { 5, "x=7;p=&x;pp=&p", "7\n", .look = "x" } } },

    { "float*", false,
      "float f[3] = {1.5, 2.5, 3.5};\n"
      "float *p = f;\n"
      "p++;\n"
      "printf(\"%.1f\\n\", *p);",
      { { 1, "f=[1.5,2.5,3.5]" },
        { 2, "f=[1.5,2.5,3.5];p=&f[0]" },
        { 3, "f=[1.5,2.5,3.5];p=&f[1]" },
        { 4, "f=[1.5,2.5,3.5];p=&f[1]", "2.5\n", .look = "f[1]" } } },

    { "<stdlib.h> malloc free", false,
      "#include <stdio.h>\n"
      "#include <stdlib.h>\n"
      "int main(void) {\n"
      "    int *p = malloc(3 * sizeof(int));\n"
      "    p[0] = 4;\n"
      "    p[1] = 5;\n"
      "    p[2] = 6;\n"
      "    printf(\"%d\\n\", p[1]);\n"
      "    free(p);\n"
      "    return 0;\n"
      "}",
      { { 3, "" },
        { 4, "p=&#1;#1=[?,?,?]" },
        { 5, "p=&#1;#1=[4,?,?]", .look = "#1[0]" },
        { 6, "p=&#1;#1=[4,5,?]", .look = "#1[1]" },
        { 7, "p=&#1;#1=[4,5,6]", .look = "#1[2]" },
        { 8, "p=&#1;#1=[4,5,6]", "5\n", .look = "#1[1]" },
        { 9, "p=&#1;#1=~[4,5,6]" },
        { 10, "p=&#1;#1=~[4,5,6]" } } },

    // ---- C++ ----
    { "<iostream> using namespace std cout << endl", true,
      "#include <iostream>\n"
      "using namespace std;\n"
      "int main() {\n"
      "    cout << \"Hi\" << endl;\n"
      "    cout << 1 + 2 << endl;\n"
      "    return 0;\n"
      "}",
      { { 3, "" }, { 4, "", "Hi\n" }, { 5, "", "3\n", .eval = "1 + 2", .result = "3" }, { 6, "" } } },

    { "cin >>", true,
      "int n;\n"
      "cin >> n;\n"
      "cout << n * 2 << endl;",
      { { 1, "n=?" }, { 2, "n=21", .typed = "21\n", .look = "n" }, { 3, "n=21", "42\n", .eval = "21 * 2", .result = "42" } } },

    { "new delete", true,
      "int *p = new int(5);\n"
      "*p = *p + 1;\n"
      "cout << *p << endl;\n"
      "delete p;",
      { { 1, "p=&#1;#1=5" },
        { 2, "p=&#1;#1=6", .eval = "5 + 1", .result = "6", .look = "#1" },
        { 3, "p=&#1;#1=6", "6\n", .look = "#1" },
        { 4, "p=&#1;#1=~6" } } },

    { "nullptr", true,
      "int *p = nullptr;\n"
      "int x = 4;\n"
      "p = &x;\n"
      "cout << *p << endl;",
      { { 1, "p=NULL" }, { 2, "p=NULL;x=4" }, { 3, "p=&x;x=4" }, { 4, "p=&x;x=4", "4\n", .look = "x" } } },

    { "class public", true,
      "class Dog {\n"
      "public:\n"
      "    int age;\n"
      "};\n"
      "int main() {\n"
      "    Dog d;\n"
      "    d.age = 3;\n"
      "    cout << d.age << endl;\n"
      "    return 0;\n"
      "}",
      { { 5, "" }, { 6, "d.age=?" }, { 7, "d.age=3" }, { 8, "d.age=3", "3\n", .look = "d.age" }, { 9, "d.age=3" } } },

    { "::", true,
      "class Box {\n"
      "public:\n"
      "    int w;\n"
      "    int area();\n"
      "};\n"
      "int Box::area() {\n"
      "    return w * w;\n"
      "}\n"
      "int main() {\n"
      "    Box b;\n"
      "    b.w = 3;\n"
      "    cout << b.area() << endl;\n"
      "    return 0;\n"
      "}",
      { { 9, "" },
        { 10, "b.w=?" },
        { 11, "b.w=3" },
        { 12, "b.w=3" },
        { 7, "b.w=3", .eval = "3 * 3", .result = "9" },
        { 12, "b.w=3", "9\n" },
        { 13, "b.w=3" } } },

    { "->", true,
      "struct Point {\n"
      "    int x;\n"
      "};\n"
      "int main() {\n"
      "    Point pt;\n"
      "    Point *p = &pt;\n"
      "    p->x = 4;\n"
      "    cout << pt.x << endl;\n"
      "    return 0;\n"
      "}",
      { { 4, "" },
        { 5, "pt.x=?" },
        { 6, "pt.x=?;p=&pt" },
        { 7, "pt.x=4;p=&pt", .look = "pt.x" },
        { 8, "pt.x=4;p=&pt", "4\n", .look = "pt.x" },
        { 9, "pt.x=4;p=&pt" } } },

    { "struct", true,
      "struct Pair {\n"
      "    int a;\n"
      "    int b;\n"
      "};\n"
      "int main() {\n"
      "    Pair p;\n"
      "    p.a = 1;\n"
      "    p.b = 2;\n"
      "    cout << p.a + p.b << endl;\n"
      "    return 0;\n"
      "}",
      { { 5, "" },
        { 6, "p.a=?;p.b=?" },
        { 7, "p.a=1;p.b=?" },
        { 8, "p.a=1;p.b=2" },
        { 9, "p.a=1;p.b=2", "3\n", .eval = "1 + 2", .result = "3" },
        { 10, "p.a=1;p.b=2" } } },

    { "<cmath> sqrt", true,
      "#include <cmath>\n"
      "#include <iostream>\n"
      "using namespace std;\n"
      "int main() {\n"
      "    double r = sqrt(25.0);\n"
      "    cout << r << endl;\n"
      "    return 0;\n"
      "}",
      { { 4, "" }, { 5, "r=5", .eval = "sqrt(25.0)", .result = "5" }, { 6, "r=5", "5\n", .look = "r" }, { 7, "r=5" } } },

    { "this", true,
      "class Counter {\n"
      "public:\n"
      "    int n;\n"
      "    void add() {\n"
      "        this->n++;\n"
      "    }\n"
      "};\n"
      "int main() {\n"
      "    Counter c;\n"
      "    c.n = 1;\n"
      "    c.add();\n"
      "    cout << c.n << endl;\n"
      "    return 0;\n"
      "}",
      { { 8, "" },
        { 9, "c.n=?" },
        { 10, "c.n=1" },
        { 11, "c.n=1" },
        { 5, "c.n=2;this=&c", .look = "c.n" },
        { 12, "c.n=2", "2\n", .look = "c.n" },
        { 13, "c.n=2" } } },

    { "private", true,
      "class Account {\n"
      "private:\n"
      "    int money;\n"
      "public:\n"
      "    void set(int m) { money = m; }\n"
      "    int get() { return money; }\n"
      "};\n"
      "int main() {\n"
      "    Account a;\n"
      "    a.set(50);\n"
      "    cout << a.get() << endl;\n"
      "    return 0;\n"
      "}",
      { { 8, "" },
        { 9, "!a.money=?" },
        { 10, "!a.money=?" },
        { 5, "!a.money=50;m=50", .look = "a.money" },
        { 11, "!a.money=50" },
        { 6, "!a.money=50", .look = "a.money" },
        { 11, "!a.money=50", "50\n" },
        { 12, "!a.money=50" } } },

    { "static", true,
      "class Dog {\n"
      "public:\n"
      "    static int count;\n"
      "    Dog() { count++; }\n"
      "};\n"
      "int Dog::count = 0;\n"
      "int main() {\n"
      "    Dog a;\n"
      "    Dog b;\n"
      "    cout << Dog::count << endl;\n"
      "    return 0;\n"
      "}",
      { { 7, "Dog::count=0" },
        { 8, "Dog::count=0" },
        { 4, "Dog::count=1" },
        { 9, "Dog::count=1" },
        { 4, "Dog::count=2" },
        { 10, "Dog::count=2", "2\n", .look = "Dog::count" },
        { 11, "Dog::count=2" } } },

    { "<cstring> strlen strcpy", true,
      "#include <cstring>\n"
      "#include <iostream>\n"
      "using namespace std;\n"
      "int main() {\n"
      "    char a[10] = \"hey\";\n"
      "    char b[10];\n"
      "    strcpy(b, a);\n"
      "    cout << strlen(b) << endl;\n"
      "    return 0;\n"
      "}",
      { { 4, "" },
        { 5, "a=[h,e,y,\\0,...]" },
        { 6, "a=[h,e,y,\\0,...];b=[?,?,?,?,...]" },
        { 7, "a=[h,e,y,\\0,...];b=[h,e,y,\\0,...]", .look = "b" },
        { 8, "a=[h,e,y,\\0,...];b=[h,e,y,\\0,...]", "3\n", .eval = "strlen(b)", .result = "3" },
        { 9, "a=[h,e,y,\\0,...];b=[h,e,y,\\0,...]" } } },
};

#define DEMO_COUNT ((int)(sizeof demos / sizeof demos[0]))

// Whether a space-separated list holds word.
static bool ListHolds(const char *list, const char *word)
{
    size_t length = strlen(word);
    for (const char *p = list; *p;) {
        while (*p == ' ') p++;
        const char *end = p;
        while (*end && *end != ' ') end++;
        if ((size_t)(end - p) == length && strncmp(p, word, length) == 0) return true;
        p = end;
    }
    return false;
}

const PieceDemo *PieceDemoFind(const char *piece)
{
    if (!piece || !piece[0]) return NULL;
    for (int i = 0; i < DEMO_COUNT; i++)
        if (ListHolds(demos[i].pieces, piece)) return &demos[i];
    return NULL;
}

const PieceDemo *PieceDemoAt(int n)
{
    return n >= 0 && n < DEMO_COUNT ? &demos[n] : NULL;
}

int PieceDemoFrameCount(const PieceDemo *demo)
{
    int count = 0;
    while (count < DEMO_FRAMES_MAX && demo->frames[count].line > 0) count++;
    return count;
}

bool PieceDemoIsProgram(const PieceDemo *demo)
{
    return strstr(demo->code, "main(") != NULL;
}

// ---------------------------------------------------------------------------
// Memory text

// Reads one value of an array (up to the next ',' or ']') into cell.
static const char *ReadCell(const char *p, char *cell)
{
    int n = 0;
    while (*p && *p != ',' && *p != ']') {
        if (n < DEMO_VALUE - 1) cell[n++] = *p;
        p++;
    }
    cell[n] = '\0';
    return p;
}

// Reads a value (after the '=') into var.
static void ReadValue(const char *value, int length, DemoVar *var)
{
    char text[96];
    snprintf(text, sizeof text, "%.*s", length, value);
    const char *p = text;
    if (*p == '~') {
        var->freed = true;
        p++;
    }
    if (strcmp(p, "NULL") == 0) {
        var->kind = DEMO_NULL;
    } else if (*p == '&') {
        var->kind = DEMO_POINTER;
        p++;
        int n = 0;
        while (*p && *p != '[' && n < DEMO_NAME - 1) var->target[n++] = *p++;
        var->target[n] = '\0';
        var->targetCell = *p == '[' ? atoi(p + 1) : -1;
    } else if (*p == '[') {
        var->kind = DEMO_ARRAY;
        p++;
        while (*p && *p != ']' && var->cellCount < DEMO_CELLS) {
            p = ReadCell(p, var->cells[var->cellCount++]);
            if (*p == ',') p++;
        }
    } else {
        var->kind = DEMO_PLAIN;
        snprintf(var->cells[0], DEMO_VALUE, "%.*s", DEMO_VALUE - 1, p);
        var->cellCount = 1;
    }
}

int DemoMemoryParse(const char *memory, DemoVar *out, int max)
{
    int count = 0;
    for (const char *p = memory ? memory : ""; *p && count < max;) {
        const char *end = strchr(p, ';');
        if (!end) end = p + strlen(p);
        const char *equals = memchr(p, '=', (size_t)(end - p));
        if (equals) {
            DemoVar *var = &out[count++];
            memset(var, 0, sizeof *var);
            var->targetCell = -1;
            const char *name = p;
            if (*name == '!') {
                var->locked = true;
                name++;
            }
            snprintf(var->name, DEMO_NAME, "%.*s", (int)(equals - name), name);
            var->block = var->name[0] == '#';
            ReadValue(equals + 1, (int)(end - equals - 1), var);
        }
        p = *end ? end + 1 : end;
    }
    return count;
}
