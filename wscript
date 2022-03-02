# -*- Mode: python; py-indent-offset: 4; indent-tabs-mode: nil; coding: utf-8; -*-

# def options(opt):
#     pass

# def configure(conf):
#     conf.check_nonfatal(header_name='stdint.h', define_name='HAVE_STDINT_H')

def build(bld):
    module = bld.create_ns3_module('spdash', ['internet','config-store','stats', 'mobility'])
    module.source = [
#         'model/circular-buffer.cc',
        'model/http/http-common-request-response.cc',
        'model/http/http-server-base-request-handler.cc',
        'model/http/http-server-simple-request-handler.cc',
        'model/http/http-server.cc',
        
        'model/http/http-client-collection.cc',
        'model/http/http-client-basic.cc',
        
        'model/spdash/spdash-request-handler.cc',
        # 'model/spdash/spdash-file-downloader.cc',
        'model/spdash/spdash-video-player.cc',

        'model/dash/dash-request-handler.cc',
        'model/dash/dash-file-downloader.cc',
        'model/dash/dash-video-player.cc',
        
        'model/mobility/constant-speed-zigzag-box-mobility-model.cc',
        
        'helper/http-helper.cc',
        'helper/spdash-helper.cc',
        'helper/dash-helper.cc',
        ]

#     module_test = bld.create_ns3_module_test_library('spdash')
#     module_test.source = [
# #         'test/spdash-test-suite.cc',
#         ]

    headers = bld(features='ns3header')
    headers.module = 'spdash'
    headers.source = [
        'model/util/nlohmann_json.h',
#         'model/circular-buffer.h',
        'model/http/ext-callback.h',
        'model/http/http-common.h',
        'model/http/http-common-request-response.h',
        
        'model/http/http-server-base-request-handler.h',
        'model/http/http-server-simple-request-handler.h',
        'model/http/http-server.h',
        'model/http/http-client-collection.h',
        'model/http/http-client-basic.h',
        
#         'model/spdash/spdash-common.h',
        'model/spdash/spdash-request-handler.h',
        # 'model/spdash/spdash-file-downloader.h',
        'model/spdash/spdash-video-player.h',

        'model/dash/dash-common.h',
        'model/dash/dash-request-handler.h',
        'model/dash/dash-file-downloader.h',
        'model/dash/dash-video-player.h',
        
        'model/mobility/constant-speed-zigzag-box-mobility-model.h',
        
        'helper/http-helper.h',
        'helper/spdash-helper.h',
        'helper/dash-helper.h',
        ]

    if bld.env.ENABLE_EXAMPLES:
        bld.recurse('examples')

    # bld.ns3_python_bindings()

