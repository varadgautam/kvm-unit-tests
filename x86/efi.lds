/* Same as gnu-efi's elf_x86_64_fbsd_efi.lds. */
OUTPUT_FORMAT("elf64-x86-64", "elf64-x86-64", "elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)
ENTRY(_efi_pe_entry)
SECTIONS
{
	. = 0;
	ImageBase = .;
	.hash : { *(.hash) }
	.gnu.hash : { *(.gnu.hash) }
	. = ALIGN(4096);
	.text :
	{
		_text = .;
		*(.text)
		*(.text.*)
		*(.gnu.linkonce.t.*)
		. = ALIGN(16);
	}
	_etext = .;
	_text_size = . - _text;
	. = ALIGN(4096);
	.reloc :
	{
		LONG(_data);
		LONG(10);
		SHORT(0);
		*(.reloc)
	}
	. = ALIGN(4096);
	.data :
	{
		_data = .;
		exception_table_start = .;
		*(.data.ex)
		exception_table_end = .;
		*(.rodata*)
		*(.got.plt)
		*(.got)
		*(.data*)
		*(.sdata)
		*(.sbss)
		*(.scommon)
		*(.dynbss)
		*(.bss)
		*(COMMON)
		*(.rel.local)
	}
	.note.gnu.build-id : { *(.note.gnu.build-id) }
	edata = .;
	_data_size = . - _etext;
	. = ALIGN(4096);
	.dynamic  : { *(.dynamic) }
	. = ALIGN(4096);
	.rela :
	{
		*(.rela.data*)
		*(.rela.got)
		*(.rela.stab)
	}
	. = ALIGN(4096);
	.dynsym   : { *(.dynsym) }
	. = ALIGN(4096);
	.dynstr   : { *(.dynstr) }
	. = ALIGN(4096);
	.comment 0 : { *(.comment) }
}
