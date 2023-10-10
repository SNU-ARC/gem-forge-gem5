
microcode = '''
def macroop STREAM_CFG_IDX_BASE
{
    stream_cfg_idx_base reg imm
};

def macroop STREAM_CFG_IDX_GRAN
{
    stream_cfg_idx_gran reg imm
};

def macroop STREAM_CFG_VAL_BASE
{
    stream_cfg_val_base reg imm
};

def macroop STREAM_CFG_VAL_GRAN
{
    stream_cfg_val_gran reg imm
};

def macroop STREAM_INPUT_OFFSET_BEGIN
{
    stream_input_offset_begin reg imm
};

def macroop STREAM_INPUT_OFFSET_END
{
    stream_input_offset_end reg imm
};

def macroop STREAM_TERMINATE
{
    stream_terminate imm
};

'''
