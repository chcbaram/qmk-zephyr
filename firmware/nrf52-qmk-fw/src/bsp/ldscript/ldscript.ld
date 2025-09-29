

SECTION_DATA_PROLOGUE(_TEXT_SECTION_NAME,,)
{
  . = ALIGN(8);
  _smodule = .;
  KEEP (*(.module))
  KEEP (*(.module*))
  _emodule = .;

} GROUP_LINK_IN(ROMABLE_REGION)