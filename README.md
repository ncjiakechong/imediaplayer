# mediaplayer
A C++ media player, similar to Qt Multimedia, but without the Qt dependency.  
The current code implements the core iObject and a media player (based on GStreamer).  
A C++ INC router is planned for the next phase.

Key features of the core iObject:

A. Signal/Slot mechanism:
  - Automatically checks argument compatibility between signals and slots.
  - Supports lambda expressions and function pointers.
  - Supports signal transmission.

        □----------------Example----------------□
        // define test class and signal as usual
        class TestSignals : public iObject {
            IX_OBJECT(TestSignals)
        public:
            void tst_sig_int1(int arg1) ISIGNAL(tst_sig_int1, arg1);
        };

        // connect signal to a slot function
        iObject::connect(&tst_sig, &TestSignals::tst_sig_int1, &tst_funcSlot, &TestFunctionSlot::tst_slot_int1);
        
        //emit the signal and slot will call by automatically
        IEMIT tst_sig.tst_sig_int1(1);
        □---------------------------------------□

B. Properties:
  - Supports setting and getting properties, and registering signal listeners for property changes.

        □----------------Example----------------□
        // to declare a property in a class with read/write/singal functions
        IPROPERTY_ITEM("testProperty", IREAD testProperty, IWRITE setTestProperty, INOTIFY testPropertyChanged)
        
        tst_sharedObj->observeProperty("testProperty", tst_sharedObj, &TestObject::tst_slot_int1);
        tst_sharedObj->setProperty("testProperty", iVariant(5.0));
        □---------------------------------------□

C. Metadata checking.

        □----------------Example----------------□
        iobject_cast<TestObject*>(tst_objcost);
        □---------------------------------------□

D. Implements utility objects for threads, event loops, timers, and memory pools.
