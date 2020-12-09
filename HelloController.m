// clang -fsyntax-only -Xclang -ast-dump ~/Desktop/HelloController.m >> ~/Desktop/ast.txt

#import <Foundation/Foundation.h>

// 注解
#define ANNOTATION(content) __attribute__((annotate(content)))
 
@interface HelloWorld : NSObject
@end
@implementation HelloWorld

// 需要自定义logger
//- (void)sayHi1:(NSString *)msg __attribute__((logger("id", "content"))) {
//    NSLog(@"Hello %@", msg);
//}

- (void)sayHi2:(NSString *)msg ANNOTATION("【注解插入的内容】") {
    NSLog(@"Hello %@", msg);
}

- (void)sayH3:(NSString *)msg {
    NSLog(@"Hello %@", msg);
}

@end
