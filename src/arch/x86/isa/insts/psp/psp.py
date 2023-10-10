
microcode = '''
def macroop STREAM_CFG_IDX_BASE_R_I
{
    stream_cfg_idx_base reg, imm
};

def macroop STREAM_CFG_IDX_GRAN_R_I
{
    stream_cfg_idx_gran reg, imm
};

def macroop STREAM_CFG_VAL_BASE_R_I
{
    stream_cfg_val_base reg, imm
};

def macroop STREAM_CFG_VAL_GRAN_R_I
{
    stream_cfg_val_gran reg, imm
};

def macroop STREAM_INPUT_OFFSET_BEGIN_R_I
{
    stream_input_offset_begin reg, imm
};

def macroop STREAM_INPUT_OFFSET_END_R_I
{
    stream_input_offset_end reg, imm
};

def macroop STREAM_TERMINATE_I
{
    stream_terminate imm
};

'''
