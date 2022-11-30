use std::env;

fn main() {
    let args: Vec<_> = env::args().collect();
    for arg in args {
        println!("{}", arg);
    }
}
