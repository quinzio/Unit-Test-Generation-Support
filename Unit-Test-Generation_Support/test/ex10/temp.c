// ex10
// Unions
struct s_s{
        int sa;
        long sb;
}s_s1;

union u1 {
	int a;
	long b;
    struct s_s s1;
}vu1;
/*
union u2 {
	int c;
	long d;
	union u1 v2u1;
}vu2;
*/
int main()
{
	vu1.a = 1111;
    s_s1.sb = vu1.b;
	return 0;
}