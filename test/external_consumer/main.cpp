#include <core/kernel/iobject.h>

class ExternalObject : public iShell::iObject {};

int main()
{
    ExternalObject object;
    return object.metaObject() ? 0 : 1;
}