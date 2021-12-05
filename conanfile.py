import conans

class ForTheKingUtilsConan(conans.ConanFile):
    settings = "os", "compiler", "build_type", "arch"

    requires = (
        "sfml/2.5.1",
        "imgui-sfml/2.3@bincrafters/stable", # Includes embedded imgui
        "protobuf/3.17.1",
    )

    default_options = {
        "sfml:shared" : False,
        "imgui-sfml:shared" : False,
        "protobuf:shared" : False,
        
        "sfml:audio" : False,
        "sfml:network" : False,

        "sfml:graphics" : True,
        "sfml:window" : True,
    }

    def configure(self):
        if self.settings.build_type == "Debug":
            self.options["imgui-sfml"].imgui_demo = True
