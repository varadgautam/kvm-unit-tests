#include <alloc_phys.h>
#include <linux/uefi.h>
#include <elf.h>

unsigned long __efiapi efi_main(efi_handle_t handle, efi_system_table_t *sys_tab);
efi_system_table_t *efi_system_table = NULL;

extern char ImageBase;
extern char _DYNAMIC;

static void efi_free_pool(void *ptr)
{
	efi_bs_call(free_pool, ptr);
}

static efi_status_t efi_get_memory_map(struct efi_boot_memmap *map)
{
	efi_memory_desc_t *m = NULL;
	efi_status_t status;
	unsigned long key = 0, map_size = 0, desc_size = 0;

	status = efi_bs_call(get_memory_map, &map_size,
			     NULL, &key, &desc_size, NULL);
	if (status != EFI_BUFFER_TOO_SMALL || map_size == 0)
		goto out;

	/* Pad map_size with additional descriptors so we don't need to
	 * retry. */
	map_size += 4 * desc_size;
	*map->buff_size = map_size;
	status = efi_bs_call(allocate_pool, EFI_LOADER_DATA,
			     map_size, (void **)&m);
	if (status != EFI_SUCCESS)
		goto out;

	/* Get the map. */
	status = efi_bs_call(get_memory_map, &map_size,
			     m, &key, &desc_size, NULL);
	if (status != EFI_SUCCESS) {
		efi_free_pool(m);
		goto out;
	}

	*map->desc_size = desc_size;
	*map->map_size = map_size;
	*map->key_ptr = key;
out:
	*map->map = m;
	return status;
}

static efi_status_t efi_exit_boot_services(void *handle,
					   struct efi_boot_memmap *map)
{
	return efi_bs_call(exit_boot_services, handle, *map->key_ptr);
}

static efi_status_t exit_efi(void *handle)
{
	unsigned long map_size = 0, key = 0, desc_size = 0, buff_size;
	efi_memory_desc_t *mem_map, *md, *conventional = NULL;
	efi_status_t status;
	unsigned num_ents, i;
	unsigned long pages = 0;
	struct efi_boot_memmap map;

	map.map = &mem_map;
	map.map_size = &map_size;
	map.desc_size = &desc_size;
	map.desc_ver = NULL;
	map.key_ptr = &key;
	map.buff_size = &buff_size;

	status = efi_get_memory_map(&map);
	if (status != EFI_SUCCESS)
		return status;

	status = efi_exit_boot_services(handle, &map);
	if (status != EFI_SUCCESS) {
		efi_free_pool(mem_map);
		return status;
	}

	/* Use the largest EFI_CONVENTIONAL_MEMORY range for phys_alloc_init. */
	num_ents = map_size / desc_size;
	for (i = 0; i < num_ents; i++) {
		md = (efi_memory_desc_t *) (((u8 *) mem_map) + i * (desc_size));

		if (md->type == EFI_CONVENTIONAL_MEMORY && md->num_pages > pages) {
			conventional = md;
			pages = md->num_pages;
		}
	}
	phys_alloc_init(conventional->phys_addr,
			conventional->num_pages << EFI_PAGE_SHIFT);

	return EFI_SUCCESS;
}

static efi_status_t elf_reloc(unsigned long image_base, unsigned long dynamic)
{
	long relsz = 0, relent = 0;
	Elf64_Rel *rel = 0;
	Elf64_Dyn *dyn = (Elf64_Dyn *) dynamic;
	unsigned long *addr;
	int i;

	for (i = 0; dyn[i].d_tag != DT_NULL; i++) {
		switch (dyn[i].d_tag) {
		case DT_RELA:
			rel = (Elf64_Rel *)
				((unsigned long) dyn[i].d_un.d_ptr + image_base);
			break;
		case DT_RELASZ:
			relsz = dyn[i].d_un.d_val;
			break;
		case DT_RELAENT:
			relent = dyn[i].d_un.d_val;
			break;
		default:
			break;
		}
	}

	if (!rel && relent == 0)
		return EFI_SUCCESS;

	if (!rel || relent == 0)
		return EFI_LOAD_ERROR;

	while (relsz > 0) {
		/* apply the relocs */
		switch (ELF64_R_TYPE (rel->r_info)) {
		case R_X86_64_NONE:
			break;
		case R_X86_64_RELATIVE:
			addr = (unsigned long *) (image_base + rel->r_offset);
			*addr += image_base;
			break;
		default:
			break;
		}
		rel = (Elf64_Rel *) ((char *) rel + relent);
		relsz -= relent;
	}
	return EFI_SUCCESS;
}

unsigned long __efiapi efi_main(efi_handle_t handle, efi_system_table_t *sys_tab)
{
	unsigned long image_base, dyn;
	efi_system_table = sys_tab;

	exit_efi(handle);

	image_base = (unsigned long) &ImageBase;
	dyn = image_base + (unsigned long) &_DYNAMIC;

	/* The EFI loader does not handle ELF relocations, so fixup
	 * .dynamic addresses before proceeding any further. */
	elf_reloc(image_base, dyn);

	start64();

	return 0;
}
