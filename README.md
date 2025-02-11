# mediaplayer
A C++ media player, similar to Qt Multimedia, but without the Qt dependency.
The current code implements the core iObject and a media player (based on GStreamer).  
A C++ INC is planned for the next phase.

Key features of the core iObject:

A. Signal/Slot mechanism:
  - Automatically checks argument compatibility between signals and slots.
  - Supports lambda expressions and function pointers.
  - Supports signal transmission.

  □----------------Example----------------□
  
  iObject::connect(&tst_sig, &TestSignals::tst_sig_int1, &tst_funcSlot, &TestFunctionSlot::tst_slot_int1);
  
  IEMIT tst_sig.tst_sig_int1(1);
  
  □---------------------------------------□

B. Properties:
  - Supports setting and getting properties, and registering signal listeners for property changes.
  
  □----------------Example----------------□
  
  tst_sharedObj->observeProperty("testProperty", tst_sharedObj, &TestObject::tst_slot_int1);
  
  tst_sharedObj->setProperty("testProperty", iVariant(5.0));
  
  □---------------------------------------□
  
C. Metadata checking.

D. Implements utility objects for threads, event loops, timers, and memory pools.
