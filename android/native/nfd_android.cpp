// The Android ROM picker is implemented through ACTION_OPEN_DOCUMENT in Java.
// RT64's desktop inspector is not exposed in this build. Its optional file
// dialogs return cancellation rather than pulling GTK or X11 into the APK.
#include "nfd.h"
#include <cstdlib>
extern "C" {
void NFD_FreePathN(nfdnchar_t* p) { std::free(p); }
void NFD_FreePathU8(nfdu8char_t* p) { std::free(p); }
nfdresult_t NFD_Init(void) { return NFD_OKAY; }
void NFD_Quit(void) {}
const char* NFD_GetError(void) { return "Use the Android document picker."; }
void NFD_ClearError(void) {}
#define CANCEL_DIALOG(suffix, chartype, filtertype) \
nfdresult_t NFD_OpenDialog##suffix(chartype** p, const filtertype*, nfdfiltersize_t, const chartype*) { if(p) *p=nullptr; return NFD_CANCEL; } \
nfdresult_t NFD_OpenDialogMultiple##suffix(const nfdpathset_t** p, const filtertype*, nfdfiltersize_t, const chartype*) { if(p) *p=nullptr; return NFD_CANCEL; } \
nfdresult_t NFD_SaveDialog##suffix(chartype** p, const filtertype*, nfdfiltersize_t, const chartype*, const chartype*) { if(p) *p=nullptr; return NFD_CANCEL; } \
nfdresult_t NFD_PickFolder##suffix(chartype** p, const chartype*) { if(p) *p=nullptr; return NFD_CANCEL; } \
nfdresult_t NFD_PathSet_GetPath##suffix(const nfdpathset_t*, nfdpathsetsize_t, chartype** p) { if(p) *p=nullptr; return NFD_ERROR; } \
void NFD_PathSet_FreePath##suffix(const chartype* p) { std::free(const_cast<chartype*>(p)); } \
nfdresult_t NFD_PathSet_EnumNext##suffix(nfdpathsetenum_t*, chartype** p) { if(p) *p=nullptr; return NFD_OKAY; }
CANCEL_DIALOG(N, nfdnchar_t, nfdnfilteritem_t)
CANCEL_DIALOG(U8, nfdu8char_t, nfdu8filteritem_t)
#undef CANCEL_DIALOG
nfdresult_t NFD_PathSet_GetCount(const nfdpathset_t*, nfdpathsetsize_t* n) { if(n) *n=0; return NFD_OKAY; }
nfdresult_t NFD_PathSet_GetEnum(const nfdpathset_t*, nfdpathsetenum_t* p) { if(p) p->ptr=nullptr; return NFD_OKAY; }
void NFD_PathSet_FreeEnum(nfdpathsetenum_t*) {}
void NFD_PathSet_Free(const nfdpathset_t*) {}
}
