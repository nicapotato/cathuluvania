#ifndef ACTS_METADATA_H
#define ACTS_METADATA_H

#define ACTS_METADATA(X) \
    X(green_act, "green-act", "Green Act") \
    X(dark_act,  "dark-act",  "Dark Act") \
    X(template_act, "template", "Template")

#define DEFAULT_ACT_ID "dark-act"

#endif
