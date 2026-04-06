# Photon Watch

Utilizing NtSetInformationVirtualMemory & VmRemoveFromWorkingSetInformation to remove a user-defined range of pages from the current process working set.<br>
<br>
For page watches EmptyWorkingSet could be used to clear the whole process working set, leaving the pages you want to monitor untouched for the duration of the watch.<br>
Decently sized performance overhead since the whole working set would be cleared.<br>
<br>
NtSetInformationVirtualMemory & VmRemoveFromWorkingSetInformation gives an option to define a range of pages to be removed from the working set.<br>
Making it easier to manage page watches & more performance effective.<br>
