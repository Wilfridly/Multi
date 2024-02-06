#include "stdio.h"

__attribute__((constructor)) void main()
{
	char		c;
	char		s[] = "\n Hello World! \n";

	while(1) 
        {
            tty_puts(s);
            char c = tty_getc();
	}

} // end main
