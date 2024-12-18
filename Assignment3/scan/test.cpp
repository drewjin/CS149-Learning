#include<stdio.h>
int main() {
	int a,b,c;
    // 1
	printf("请输入三角形得三条边：\n");
    // 2
	scanf("%d",&a); scanf("%d",&b); scanf("%d",&c);
    // 3
    if ((a <= 0 || b <= 0 || c <= 0) 
       || !(a + b > c && a + c > b && b + c > a)) {
        // 4
        printf("不是三角形!");
    } else {
        // 5
        if (a == b && a == c && b == c)
            // 6
            printf("等边三角形");
        // 7
        else if ((a * a + b * b == c * c) 
            || (b * b + c * c == a * a) 
            || (c * c + a * a == b * b))
            // 8
            printf("直角三角形");
        else
            // 9
            printf("普通三角形");
    }	
    // 10
    return 0;
}