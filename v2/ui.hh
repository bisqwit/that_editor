//#include <vector>

class UI
{
protected:
    double VidFPS;
    unsigned VidW;
    unsigned VidH;
    unsigned VidCellHeight;
public:
    virtual ~UI()
    {
    }

    virtual void SetMode(
        /* Width and height in character cells */
        unsigned width,
        unsigned height,
        /* Font specification */
        unsigned font_width,
        unsigned font_height,
        bool     font_x_double,
        bool     font_y_double,
        /* Special transformations */
        bool     C64mode
    ) = 0;

    //virtual std::vector<... getavailablemodes
};

class UIfontBase
{
    virtual const unsigned char* GetBitmap() const = 0;
    virtual unsigned GetIndex(char32_t c) const = 0;
};
