#pragma once
#include "stdafx.h"
#include "BaseMapper.h"
#include "PPU.h"

class MMC5 : public BaseMapper
{
private:
	uint8_t _prgRamProtect1;
	uint8_t _prgRamProtect2;

	uint8_t _fillModeTile;
	uint8_t _fillModeColor;
	uint8_t *_fillModeNametable;

	uint8_t *_emptyNametable;

	bool _verticalSplitEnabled;
	bool _verticalSplitRightSide;
	uint8_t _verticalSplitDelimiterTile;
	uint8_t _verticalSplitScroll;
	uint8_t _verticalSplitBank;

	uint8_t _multiplierValue1;
	uint8_t _multiplierValue2;

	uint8_t _nametableMapping;
	uint8_t _extendedRamMode;

	//Extended attribute mode fields (used when _extendedRamMode == 1)
	uint16_t _exAttributeLastNametableFetch;
	int8_t _exAttrLastFetchCounter;
	uint8_t _exAttrSelectedChrBank;

	uint8_t _prgMode;
	uint8_t _prgBanks[5];

	//CHR-related fields
	uint8_t _chrMode;
	uint8_t _chrUpperBits;	
	uint16_t _chrBanks[12];
	uint16_t _lastChrReg;
	bool _spriteFetch;
	bool _largeSprites;

	//IRQ counter related fields
	uint8_t _irqCounterTarget;
	bool _irqEnabled;
	int16_t _previousScanline;
	uint8_t _irqCounter;
	bool _irqPending;
	bool _ppuInFrame;

	void SwitchPrgBank(uint16_t reg, uint8_t value)
	{
		_prgBanks[reg - 0x5113] = value;
		UpdatePrgBanks();
	}

	void GetCpuBankInfo(uint16_t reg, uint8_t &bankNumber, PrgMemoryType &memoryType, uint8_t &accessType)
	{
		bankNumber = _prgBanks[reg-0x5113];
		memoryType = PrgMemoryType::PrgRom;
		if((((bankNumber & 0x80) == 0x00) && reg != 0x04) || reg == 0x00) {
			bankNumber &= 0x07;
			memoryType = PrgMemoryType::SaveRam;
			accessType = MemoryAccessType::Read;
			if(_prgRamProtect1 == 0x02 && _prgRamProtect2 == 0x01) {
				accessType |= MemoryAccessType::Write;
			}
		} else {
			accessType = MemoryAccessType::Read;
			bankNumber &= 0x7F;
		}
	}

	void UpdatePrgBanks()
	{
		uint8_t value;
		PrgMemoryType memoryType;
		uint8_t accessType;
			
		GetCpuBankInfo(0x5113, value, memoryType, accessType);
		SetCpuMemoryMapping(0x6000, 0x7FFF, value, memoryType, accessType);
			
		//PRG Bank 0
		//Mode 0,1,2 - Ignored
		//Mode 3 - Select an 8KB PRG bank at $8000-$9FFF
		if(_prgMode == 3) {
			GetCpuBankInfo(0x5114, value, memoryType, accessType);
			SetCpuMemoryMapping(0x8000, 0x9FFF, value, memoryType, accessType);
		}

		//PRG Bank 1
		//Mode 0 - Ignored
		//Mode 1,2 - Select a 16KB PRG bank at $8000-$BFFF (ignore bottom bit)
		//Mode 3 - Select an 8KB PRG bank at $A000-$BFFF
		GetCpuBankInfo(0x5115, value, memoryType, accessType);
		if(_prgMode == 1 || _prgMode == 2) {
			SetCpuMemoryMapping(0x8000, 0xBFFF, value & 0xFE, memoryType, accessType);
		} else if(_prgMode == 3) {
			SetCpuMemoryMapping(0xA000, 0xBFFF, value, memoryType, accessType);
		}

		//Mode 0,1 - Ignored
		//Mode 2,3 - Select an 8KB PRG bank at $C000-$DFFF
		if(_prgMode == 2 || _prgMode == 3) {
			GetCpuBankInfo(0x5116, value, memoryType, accessType);
			SetCpuMemoryMapping(0xC000, 0xDFFF, value, memoryType, accessType);
		}

		//Mode 0 - Select a 32KB PRG ROM bank at $8000-$FFFF (ignore bottom 2 bits)
		//Mode 1 - Select a 16KB PRG ROM bank at $C000-$FFFF (ignore bottom bit)
		//Mode 2,3 - Select an 8KB PRG ROM bank at $E000-$FFFF
		GetCpuBankInfo(0x5117, value, memoryType, accessType);
		if(_prgMode == 0) {
			SetCpuMemoryMapping(0x8000, 0xFFFF, value & 0x7C, memoryType, accessType);
		} else if(_prgMode == 1) {
			SetCpuMemoryMapping(0xC000, 0xFFFF, value & 0x7E, memoryType, accessType);
		} else if(_prgMode == 2 || _prgMode == 3) {
			SetCpuMemoryMapping(0xE000, 0xFFFF, value & 0x7F, memoryType, accessType);
		}
	}

	void SwitchChrBank(uint16_t reg, uint8_t value)
	{
		_chrBanks[reg - 0x5120] = value | (_chrUpperBits << 8);
		_lastChrReg = reg;
		UpdateChrBanks(!PPU::GetControlFlags().BackgroundEnabled && !PPU::GetControlFlags().SpritesEnabled);
	}

	void UpdateChrBanks(bool forceA = false)
	{
		_spriteFetch = IsSpriteFetch();
		_largeSprites = PPU::GetControlFlags().LargeSprites;

		bool chrA = forceA || (_largeSprites && _spriteFetch) || (!_largeSprites && _lastChrReg <= 0x5127);
		if(_chrMode == 0) {
			SetPpuMemoryMapping(0x0000, 0x1FFF, _chrBanks[chrA ? 0x07 : 0x0B] << 3);
		} else if(_chrMode == 1) {
			SetPpuMemoryMapping(0x0000, 0x0FFF, _chrBanks[chrA ? 0x03 : 0x0B] << 2);
			SetPpuMemoryMapping(0x1000, 0x1FFF, _chrBanks[chrA ? 0x07 : 0x0B] << 2);
		} else if(_chrMode == 2) {
			SetPpuMemoryMapping(0x0000, 0x07FF, _chrBanks[chrA ? 0x01 : 0x09] << 1);
			SetPpuMemoryMapping(0x0800, 0x0FFF, _chrBanks[chrA ? 0x03 : 0x0B] << 1);
			SetPpuMemoryMapping(0x1000, 0x17FF, _chrBanks[chrA ? 0x05 : 0x09] << 1);
			SetPpuMemoryMapping(0x1800, 0x1FFF, _chrBanks[chrA ? 0x07 : 0x0B] << 1);
		} else if(_chrMode == 3) {
			SetPpuMemoryMapping(0x0000, 0x03FF, _chrBanks[chrA ? 0x00 : 0x08]);
			SetPpuMemoryMapping(0x0400, 0x07FF, _chrBanks[chrA ? 0x01 : 0x09]);
			SetPpuMemoryMapping(0x0800, 0x0BFF, _chrBanks[chrA ? 0x02 : 0x0A]);
			SetPpuMemoryMapping(0x0C00, 0x0FFF, _chrBanks[chrA ? 0x03 : 0x0B]);
			SetPpuMemoryMapping(0x1000, 0x13FF, _chrBanks[chrA ? 0x04 : 0x08]);
			SetPpuMemoryMapping(0x1400, 0x17FF, _chrBanks[chrA ? 0x05 : 0x09]);
			SetPpuMemoryMapping(0x1800, 0x1BFF, _chrBanks[chrA ? 0x06 : 0x0A]);
			SetPpuMemoryMapping(0x1C00, 0x1FFF, _chrBanks[chrA ? 0x07 : 0x0B]);
		}
	}

	virtual void NotifyVRAMAddressChange(uint16_t addr)
	{
		if(_spriteFetch != IsSpriteFetch() || _largeSprites != PPU::GetControlFlags().LargeSprites) {
			if(PPU::GetControlFlags().BackgroundEnabled || PPU::GetControlFlags().SpritesEnabled) {
				UpdateChrBanks();
			}
		}

		int16_t currentScanline = PPU::GetCurrentScanline();
		if(currentScanline != _previousScanline) {
			if(currentScanline < _previousScanline) {
				_ppuInFrame = false;
			}

			if(currentScanline >= 239 || currentScanline < 0) {
				_ppuInFrame = false;
			} else {
				if(!_ppuInFrame) {
					_ppuInFrame = true;
					_irqCounter = 0;
					_irqPending = false;
					CPU::ClearIRQSource(IRQSource::External);
				} else {
					_irqCounter++;
					if(_irqCounter == _irqCounterTarget) {
						_irqPending = true;
						if(_irqEnabled) {
							CPU::SetIRQSource(IRQSource::External);
						}
					}
				}
			}
			_previousScanline = currentScanline;
		}
	}

	void SetNametableMapping(uint8_t value)
	{
		_nametableMapping = value;

		uint8_t* nametables[4] = { 
			_nesNametableRam[0],  //"0 - On-board VRAM page 0"
			_nesNametableRam[1],  //"1 - On-board VRAM page 1"
			_extendedRamMode <= 1 ? _workRam : _emptyNametable, //"2 - Internal Expansion RAM, only if the Extended RAM mode allows it ($5104 is 00/01); otherwise, the nametable will read as all zeros,"
			_fillModeNametable //"3 - Fill-mode data"
		};

		SetMirroringType(nametables[value & 0x03], nametables[(value >> 2) & 0x03], nametables[(value >> 4) & 0x03], nametables[(value >> 6) & 0x03]);
	}

	void SetExtendedRamMode(uint8_t mode)
	{
		_extendedRamMode = mode;

		if(_extendedRamMode <= 1) {
			//"Mode 0/1 - Not readable (returns open bus), can only be written while the PPU is rendering (otherwise, 0 is written)"
			//See overridden WriteRam function for implementation
			SetCpuMemoryMapping(0x5C00, 0x5FFF, 0, PrgMemoryType::WorkRam, MemoryAccessType::Write);
		} else if(_extendedRamMode == 2) {
			//"Mode 2 - Readable and writable"
			SetCpuMemoryMapping(0x5C00, 0x5FFF, 0, PrgMemoryType::WorkRam, MemoryAccessType::ReadWrite);
		} else {
			//"Mode 3 - Read-only"
			SetCpuMemoryMapping(0x5C00, 0x5FFF, 0, PrgMemoryType::WorkRam, MemoryAccessType::Read);
		}

		SetNametableMapping(_nametableMapping);
	}

	void SetFillModeTile(uint8_t tile)
	{
		_fillModeTile = tile;
		memset(_fillModeNametable, tile, 32 * 30); //32 tiles per row, 30 rows
	}

	void SetFillModeColor(uint8_t color)
	{
		_fillModeColor = color;
		memset(_fillModeNametable + 32 * 30, color, 64); //Attribute table is 64 bytes
	}

	bool IsSpriteFetch()
	{
		return PPU::GetCurrentCycle() >= 257 && PPU::GetCurrentCycle() < 321;
	}

protected:
	virtual uint16_t GetPRGPageSize() { return 0x2000; }
	virtual uint16_t GetCHRPageSize() {	return 0x400; }
	virtual uint16_t RegisterStartAddress() { return 0x5000; }
	virtual uint16_t RegisterEndAddress() { return 0x5206; }
	virtual uint32_t GetSaveRamSize() { return 0x10000; } //Emulate as if a single 64k block of saved ram existed
	virtual uint32_t GetSaveRamPageSize() { return 0x2000; }
	virtual uint32_t GetWorkRamSize() { return 0x400; } 
	virtual uint32_t GetWorkRamPageSize() { return 0x400; }
	virtual bool AllowRegisterRead() { return true; }

	virtual void InitMapper()
	{
		_hasBattery = true;

		_chrMode = 0;
		_prgRamProtect1 = 0;
		_prgRamProtect2 = 0;
		_extendedRamMode = 0;
		_fillModeColor = 0;
		_fillModeTile = 0;
		_verticalSplitScroll = 0;
		_verticalSplitBank = 0;
		_multiplierValue1 = 0;
		_multiplierValue2 = 0;
		_chrUpperBits = 0;
		memset(_chrBanks, 0, sizeof(_chrBanks));
		_lastChrReg = 0;
		_spriteFetch = false;
		_largeSprites = false;

		_exAttrLastFetchCounter = 0;
		_exAttributeLastNametableFetch = 0;
		_exAttrSelectedChrBank = 0;
		
		_irqCounterTarget = 0;
		_irqCounter = 0;
		_irqEnabled = false;
		_previousScanline = -1;
		_ppuInFrame = false;

		_fillModeNametable = new uint8_t[0x400];
		_emptyNametable = new uint8_t[0x400];
		memset(_emptyNametable, 0, 0x400);

		//"Expansion RAM ($5C00-$5FFF, read/write)"
		SetCpuMemoryMapping(0x5C00, 0x5FFF, 0, PrgMemoryType::WorkRam);

		//"Additionally, Romance of the 3 Kingdoms 2 seems to expect it to be in 8k PRG mode ($5100 = $03)."
		WriteRegister(0x5100, 0x03);

		//"Games seem to expect $5117 to be $FF on powerup (last PRG page swapped in)."
		WriteRegister(0x5117, 0xFF);
	}

	virtual ~MMC5()
	{
		delete[] _fillModeNametable;
		delete[] _emptyNametable;
	}

	void StreamState(bool saving)
	{
		//TODO
		BaseMapper::StreamState(saving);
	}

	virtual void WriteRAM(uint16_t addr, uint8_t value)
	{
		if(addr >= 0x5C00 && addr <= 0x5FFF && _extendedRamMode <= 1) {
			PPUControlFlags flags = PPU::GetControlFlags();
			if(!flags.BackgroundEnabled && !flags.SpritesEnabled) {
				//Expansion RAM ($5C00-$5FFF, read/write)
				//Mode 0/1 - Not readable (returns open bus), can only be written while the PPU is rendering (otherwise, 0 is written)
				value = 0;
			}
		}
		BaseMapper::WriteRAM(addr, value);
	}

	virtual uint8_t ReadVRAM(uint16_t addr)
	{
		if(_extendedRamMode == 1) {
			//"In Mode 1, nametable fetches are processed normally, and can come from CIRAM nametables, fill mode, or even Expansion RAM, but attribute fetches are replaced by data from Expansion RAM."
			//"Each byte of Expansion RAM is used to enhance the tile at the corresponding address in every nametable"

			//When fetching NT data, we set a flag and then alter the VRAM values read by the PPU on the following 3 cycles (palette, tile low/high byte)
			if(addr >= 0x2000 && (addr & 0x3FF) < 0x3C0) {
				//Nametable fetches
				_exAttributeLastNametableFetch = addr & 0x03FF;
				_exAttrLastFetchCounter = 3;
			} else if(_exAttrLastFetchCounter > 0) {
				//Attribute fetches
				_exAttrLastFetchCounter--;
				switch(_exAttrLastFetchCounter) {
					case 2:
					{
						//PPU palette fetch
						//Check work ram (expansion ram) to see which tile/palette to use
						//Use InternalReadRam to bypass the fact that the ram is supposed to be write-only in mode 0/1
						uint8_t value = InternalReadRam(0x5C00 + _exAttributeLastNametableFetch);

						//"The pattern fetches ignore the standard CHR banking bits, and instead use the top two bits of $5130 and the bottom 6 bits from Expansion RAM to choose a 4KB bank to select the tile from."
						_exAttrSelectedChrBank = ((value & 0x3F) | (_chrUpperBits << 6)) % (_chrSize / 0x1000);

						//Return a byte containing the same palette 4 times - this allows the PPU to select the right palette no matter the shift value
						uint8_t palette = (value & 0xC0) >> 6;
						return palette | palette << 2 | palette << 4 | palette << 6;
					}

					case 1:
					case 0:
						//PPU tile data fetch (high byte & low byte)
						return _chrRam[_exAttrSelectedChrBank * 0x1000 + (addr & 0xFFF)];
				}
			}
		}
		return BaseMapper::ReadVRAM(addr);
	}

	void WriteRegister(uint16_t addr, uint8_t value)
	{
		if(addr >= 0x5113 && addr <= 0x5117) {
			SwitchPrgBank(addr, value);
		} else if(addr >= 0x5120 && addr <= 0x512B) {
			SwitchChrBank(addr, value);
		} else {
			switch(addr) {
				case 0x5100: _prgMode = value & 0x03; UpdatePrgBanks(); break;
				case 0x5101: _chrMode = value & 0x03; UpdateChrBanks(); break;
				case 0x5102: _prgRamProtect1 = value & 0x03; UpdatePrgBanks(); break;
				case 0x5103: _prgRamProtect2 = value & 0x03; UpdatePrgBanks(); break;
				case 0x5104: SetExtendedRamMode(value & 0x03); break;
				case 0x5105: SetNametableMapping(value); break;
				case 0x5106: SetFillModeTile(value); break;
				case 0x5107: SetFillModeColor(value & 0x03); break;
				case 0x5130: _chrUpperBits = value & 0x03; break;
				case 0x5200: 
					_verticalSplitEnabled = (value & 0x80) == 0x80; 
					_verticalSplitRightSide = (value & 0x40) == 0x40; 
					_verticalSplitDelimiterTile = (value & 0x1F);
					break;
				case 0x5201: _verticalSplitScroll = value; break;
				case 0x5202: _verticalSplitBank = value; break;
				case 0x5203: _irqCounterTarget = value; break;
				case 0x5204: 
					_irqEnabled = (value & 0x80) == 0x80;
					if(!_irqEnabled) {
						CPU::ClearIRQSource(IRQSource::External);
					} else if(_irqEnabled && _irqPending) {
						CPU::SetIRQSource(IRQSource::External);
					}
					break;
				case 0x5205: _multiplierValue1 = value; break;
				case 0x5206: _multiplierValue2 = value; break;

				default:
					break;
			}
		}
	}

	uint8_t ReadRegister(uint16_t addr)
	{
		switch(addr) {
			case 0x5204:
			{
				uint8_t value = (_ppuInFrame ? 0x40 : 0x00) | (_irqPending ? 0x80 : 0x00);
				_irqPending = false;
				CPU::ClearIRQSource(IRQSource::External);
				return value;
			}

			case 0x5205: return (_multiplierValue1*_multiplierValue2) & 0xFF;
			case 0x5206: return (_multiplierValue1*_multiplierValue2) >> 8;
		}
		return 0;
	}
};