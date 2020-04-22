CXXFLAGS += -fpermissive 
COMPONENT_SRCDIRS = .  HW src FramSPI
COMPONENT_ADD_INCLUDEDIRS = . HW inc FramSPI

COMPONENT_EMBED_FILES= ok.png nak.png meter.png
COMPONENT_EMBED_TXTFILES= challenge.html connSetup.html login.html ok.html certs/public.pem certs/prvtkey.pem
