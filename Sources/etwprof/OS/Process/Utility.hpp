#ifndef ETWP_PROCESS_UTILITY_HPP
#define ETWP_PROCESS_UTILITY_HPP

namespace ETWP {

bool IsProcessElevated ();
bool AddProfilePrivilegeToProcessToken ();
bool IsProfilePrivilegePresentInToken (bool* pResultOut);

}   // namespace ETWP

#endif  // #ifndef #ifndef ETWP_PROCESS_UTILITY_HPP