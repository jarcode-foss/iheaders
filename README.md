**notice:** this is a premature repository push, this is not tested nor finished.

#Inline Headers

This is a small program that can take header information that is inlined into your C source files and generate actual headers from them. You can consolidate both your member definitions and declarations in one place, and have blocks of code that would have normally been exposed through the header (type definitions, macros, etc).

Inline Headers (`iheaders`) is a preprocessor, ran before the C preprocessor. It simply generates a header file and can pass itself to a compiler (like GCC) or strip the source of the header information for later compilation. The syntax is very simple:

    @[HEADER PREFIX][SOURCE PREFIX] <definition or declaration>

For exposing a member (note the `@` must appear at the start of the line) - the definition or declaration does not have to immediately follow, if it is not present it will set the prefixes for the following exposed members. If you omit the square brackets after the `@` symbol, it will use the last set prefixes. Example usage:

    /* set the prefixes for the header and source */
    @[extern][noinline]
    
    @ void foobar(void) {
        fputs("this is an exposed function!", stdout); 
    }
    
    /* change prefixes again */
    @[extern][]
    
    @ int exposed_integer = 2;
    
    /* use the following prefixes only for the member immediately following,
       you can omit the brackets for the source prefix */
    
    @[API_MACRO] double foobiz(int a, int b) {
        return (a + b) / 2D;
    }

For any member, it will only read up until a `;`, '=', or a `{`, only copying the definition to the generated header.

You can also expose an entire block to the header if the `@` token is immediately followed by a block:

    @ {
        #define SOME_MACRO 42
        typedef struct {
            int foo;
            double bar;
        } foobar_t;
    }

That's it! This helps keep my code much more organized, feel free to use it. See `iheaders --help` for information on how to tweak header generation and source parsing.