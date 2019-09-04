files = ["spec_base_common.ucf"]

ucf_dict = {'wr':      "spec_base_wr.ucf",
            'onewire': "spec_base_onewire.ucf",
            'spi':     "spec_base_spi.ucf",
            'ddr3':    "spec_base_ddr3.ucf",
            'dma':     "spec_base_dma.ucf"}

for p in spec_base_ucf:
    f = ucf_dict.get(p, None)
    assert f is not None, "unknown name {} in 'spec_base_ucf'".format(p)
    files.append(f)
