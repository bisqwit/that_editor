#include <string.h>

static const char Mario[] =
//_______________""""""""""""""""________________
"                     #####            #####     "
"       #####        #########        #########  "
"      #########     '''%%'%          '''%%'%    "
"      '''%%'%      '%'%%%'%%%       '%'%%%'%%%  "
"     '%'%%%'%%%    '%''%%%'%%%      '%''%%%'%%% "
"     '%''%%%'%%    ''%%%%''''       ''%%%%''''  "
"     ''%%%%''''      %%%%%%%          %%%%%%%   "
"       %%%%%%%      ''#'''         ''''##''     "
"      ''''#' %     ''''##''      %%''''###'''%%%"
"     %''''''%%%    '''##%##%     %%% ''#%###''%%"
"    %%#'''''%%     ''''#####     %%  #######  ' "
"    ''#######      #''%%%###        #########'' "
"    '########       #'%%###        ##########'' "
"   ''### ###         ###'''       ''###   ###'' "
"   '    '''          '''''''      '''           "
"        ''''         ''''          '''          ";

unsigned char* MarioFont = 0;

void MarioGetFont()
{
    MarioFont = *(unsigned char**) MK_FP(0x0, 0x43 * 4);
}

void MarioTranslate(
    const unsigned short* model,
    unsigned short* target,
    unsigned width)
{
    memcpy(target, model, width*2);
/*
    static unsigned marioframe = 0;
    if(++marioframe == 3) marioframe = 0;
    static int mariox = -40, mariodir = 1;
    if(mariodir > 0) { if(++mariox > width*8 + 30) mariodir=-1; }
    else { if(--mariox < -40) mariodir=1; }
    mariox += mariodir;

    int basex = mariox & ~7;
    while(basex < 0) basex += 8;
    while(basex < mariox + 16)
    {
        
    }
*/
}
