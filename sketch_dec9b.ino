#include <L298NX2.h>

int m1 = 2;
int m2 = 3;
int m3 = 4;
int m4 = 5;
int M1, M2;
int DN0,DP0;
int dr;       
int spd = 150;

L298NX2 mf(m1, m2, m3, m4);

void setdr() {

   mf.setSpeed(spd);

   int M1 = 0;
   int M2 = 0;

   if (dr == 1) { M1 = 1; M2 = 1; }
   if (dr == 2) { M1 = 0; M2 = 0; }
   if (dr == 3) { M1 = 0; M2 = 1; }
   if (dr == 4) { M1 = 1; M2 = 0; }

   if (M1 == 0) mf.forwardA();
   else         mf.backwardA();

   if (M2 == 0) mf.forwardB();
   else         mf.backwardB();}

void setup() {  

}

void loop() {
  setdr();
  dr = 1;
  }