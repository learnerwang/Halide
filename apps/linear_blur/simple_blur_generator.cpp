#include "Halide.h"

namespace {

struct SimpleBlur : public Halide::Generator<SimpleBlur> {
    GeneratorParam<bool>  auto_schedule{"auto_schedule", false};
    GeneratorParam<bool>  estimate_only{"estimate_only", false};

    Input<Func>    input{"input"};
    Input<int32_t> width{"width"};
    Input<int32_t> height{"height"};
    Output<Func>   output{"output"};

    Var x{"x"}, y{"y"};
    Func blur_x{"blur_x"};

    void generate() {
        using Halide::_;

        // Since Input<Func> has no extent limits, we must specify explicit min-extent pairs
        Func in = Halide::BoundaryConditions::repeat_edge(input, {{0, width}, {0, height}});

        blur_x(x, y, _) = (in(x, y, _) + in(x+1, y, _) + in(x+2, y, _))/3;
        output(x, y, _) = (blur_x(x, y, _) + blur_x(x, y+1, _) + blur_x(x, y+2, _))/3;
    }

    void schedule() {
        if ((bool) auto_schedule || (bool) estimate_only) {
            const int W = 1536, H = 2560, C = 4;
            // Wart: Input<Func> are defined with Vars we don't know.
            // Might be x,y but might be _0,_1. Use the args() to work around.
            input.estimate(input.args()[0], 0, W)
                 .estimate(input.args()[1], 0, H);
            for (size_t i = 2; i < input.args().size(); ++i) {
                input.estimate(input.args()[i], 0, C);
            }
            width.set_estimate(W);
            height.set_estimate(H);
            output.estimate(x, 0, W)
                  .estimate(y, 0, H);
            for (size_t i = 2; i < output.args().size(); ++i) {
                output.estimate(output.args()[i], 0, C);
            }
            if (auto_schedule) {
                auto_schedule_outputs();
            }
        } else {
            Var xi("xi"), yi("yi");
            output.split(y, y, yi, 8).parallel(y).vectorize(x, 8);
            blur_x.store_at(output, y).compute_at(output, yi).vectorize(x, 8);
        }
    }
};

}  // namespace

HALIDE_REGISTER_GENERATOR(SimpleBlur, simple_blur)
