files = ["spec_template_common.ucf"]

ucf_dict = {'wr':      "spec_template_wr.ucf",
            'onewire': "spec_template_onewire.ucf",
            'spi':     "spec_template_spi.ucf",
            'ddr3':    "spec_template_ddr3.ucf"}

for p in spec_template_ucf:
    f = ucf_dict.get(p, None)
    assert f is not None, "unknown name {} in 'spec_template_ucf'".format(p)
    files.append(f)
