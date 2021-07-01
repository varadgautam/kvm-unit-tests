#include <linux/uefi.h>

unsigned long __efiapi efi_main(efi_handle_t handle, efi_system_table_t *sys_tab);
efi_system_table_t *efi_system_table = NULL;

unsigned long __efiapi efi_main(efi_handle_t handle, efi_system_table_t *sys_tab)
{
	efi_system_table = sys_tab;

	return 0;
}
