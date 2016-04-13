#Inline Headers

This is a small program that can take header information that is inlined into your C source files and generate actual headers from them. You can consolidate both your member definitions and declarations in one place, and have blocks of code that would have normally been exposed through the header (type definitions, macros, etc).

    @(HEADER PREFIX)(SOURCE PREFIX) <definition or declaration>

Alternatively, you can use square bracket syntax (`[...]`) for your prefixes, although this will confuse parsing for some editors.

For exposing a member (note the `@` must appear at the start of the line) - the definition or declaration does not have to immediately follow, if it is not present it will set the prefixes for the following exposed members. If you omit the square brackets after the `@` symbol, it will use the last set prefixes. Example usage:

```C
/* set the prefixes for the header and source */
@(extern)(noinline)
    
@ void foobar(void) {
    fputs("this is an exposed function!", stdout); 
}
    
/* change prefixes again */
@(extern)()
    
@ int exposed_integer = 2;
    
/* use the following prefixes only for the member immediately following,
   you can omit the brackets for the source prefix */
    
@(API_MACRO) double foobiz(int a, int b) {
    return (a + b) / 2D;
}
```

For any member, it will only read up until a `;`, `=`, or a `{`, only copying the definition to the generated header.

You can also expose an entire block to the header if the `@` token is immediately followed by a block:

```C
@ {
    #define SOME_MACRO 42
    typedef struct {
        int foo;
        double bar;
    } foobar_t;
}
```

##Usage

(see `iheaders --help` for a full list of options)

This program has three output modes, pipe (`-O` option), directory (`-r` and `-d` options), and single-header mode (`-s` option).

**pipe mode**: will pipe the output of all the input files into stdout. This is useful for when you are stripping header syntax for compilation, and want to pipe the sources straight into a compiler.

**directory mode**: will store the resulting headers into a directory. You can use the `-r` flag to specify the root source directory, and the headers folder will copy the directory structure of your sources when creating header files.

**single-header mode**: will combine the output of the resulting headers into a single file.

The default behaviour will create a header file in the same location for every input source file.

##Notes

Depending on the editor you are using, you may want to tweak how it parses your source code. An easy fix would be to change the token from `@` (using the `-t` flag) to a valid member name, and avoiding the use of the `[...]` syntax for prefixes.

You could also tweak your editor to treat `@` characters as a valid symbol name or whitespace. The following snippet will work in Emacs 24:

```emacs
;; ignore iheader syntax
(add-hook 'c-initialization-hook
          (lambda () (modify-syntax-entry ?@ "_" c-mode-syntax-table)))
```