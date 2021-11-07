
int main() {
    {
        int i = 0;
        int j = 0;

        // Can change a  reference but can not change value.
        int const *a = &i;
        const int* b = &i;
        
        // This is OK.
        a = &j;
        b = &j;
        // This is NG.
        // *a = 10;

    }
    {
        int i = 0;
        int j = 0;
        // Can not change a reference
        int* const b = &i;
        // This is NG.
        // b = &j;
        // This is OK.
        *b = 10;
    }
    {
        int i = 0;
        int j = 0;
        // All prohibited.
        const int* const b = &i;
        // NG
        // b = &j;
        // *b = 10;
    }

    return 0;
}