/* Ad-hoc programming editor for DOSBox -- (C) 2011-03-08 Joel Yliluoma */
void MarioTranslate(
    EditorCharType*  model,
    unsigned short* target,
    unsigned width);

void FixMarioTimer();
void InstallMario();
void DeInstallMario();

extern volatile unsigned long MarioTimer;
