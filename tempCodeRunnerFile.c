#include <stdio.h>


int main()
{

    int c,i,nwhite,nother;
    int ndigit[10];

        for(int i =0; i < 10;i++)
            ndigit[i] = 0;


        while((c=getchar()) != EOF)
        {
            if(c>='0'&&c<='9')
            {
++ndigit[c-'0'];
                printf("switch 1");
            }
                
            else if(c==' ' || c == '\t' || c=='\n')   
            {
 ++nwhite; 
                printf("switch 2");
            }   
            else
            {
++nother;
                printf("switch 3");
            }
                
        }



}

