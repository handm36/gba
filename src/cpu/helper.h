#pragma once
#include "gba.h"

// Location of each bit
#define CPSR_T_LOC 5
#define CPSR_F_LOC 6
#define CPSR_I_LOC 7

#define CPSR_T_BIT (1 << CPSR_T_LOC)
#define CPSR_F_BIT (1 << CPSR_F_LOC)
#define CPSR_I_BIT (1 << CPSR_I_LOC)

#define OVERFLOW_FLAG_LOC 28
#define CARRY_FLAG_LOC 29
#define ZERO_FLAG_LOC 30
#define SIGN_FLAG_LOC 31

#define OVERFLOW_FLAG (1 << OVERFLOW_FLAG_LOC) // V flag
#define CARRY_FLAG (1 << CARRY_FLAG_LOC)       // C flag
#define ZERO_FLAG (1 << ZERO_FLAG_LOC)         // Z flag
#define SIGN_FLAG (1 << SIGN_FLAG_LOC)         // N flag

typedef enum {
  BranchAndBranchExchange,
  BlockDataTransfer,
  BranchAndBranchWithLink,
  SoftwareInterrupt,
  Undefined,
  SingleDataTransfer,
  SingleDataSwap,
  Multiply,
  HalfwordDataTransferRegister,
  HalfwordDataTransferImmediate,
  PSRTransferMRS,
  PSRTransferMSR,
  DataProcessing,
  Unimplemented,
} Arm_Instructions;

typedef enum {
  SoftwareInterruptTHUMB,
  UnconditionalBranch,
  ConditionalBranch,
  MultipleLoadstore,
  LongBranchWithLink,
  AddOffsetToStackPointer,
  PushPopRegisters,
  LoadStoreHalfword,
  SPRelativeLoadStore,
  LoadAddress,
  LoadStoreWithImmediateOffset,
  LoadStoreWithRegisterOffset,
  LoadStoreSignExtendedByteHalfword,
  PCRelativeLoad,
  HiRegisterOperationsBranchExchange,
  ALUOperations,
  MoveCompareAddSubtractImmediate,
  AddSubtract,
  MoveShiftedRegister,
  UnimplementedTHUMB
} Thumb_Instructions;

Thumb_Instructions decode_thumb(uint16_t opcode);
Arm_Instructions decode_arm(uint32_t opcode);
int check_condition_code(uint32_t CPSR, uint8_t condition_code);
uint8_t carry_from(uint32_t x, uint32_t y, uint32_t z);
uint8_t borrow_from(int32_t x, int32_t y, int32_t z);
uint8_t overflow_from(int32_t x, int32_t y, int32_t result, uint8_t is_sub);
int current_mode_has_SPSR(GBA_CPU *cpu);
uint32_t get_current_spsr(GBA_CPU *cpu);
