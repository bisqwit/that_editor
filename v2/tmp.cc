struct foo
{
    union test
    {
        int x = 0;
        short y;
    };

    constexpr foo()  { }
};
