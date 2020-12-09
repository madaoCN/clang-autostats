#include "clang/Driver/Options.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/CodeGen/CodeGenAction.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Frontend/FrontendActions.h"

#include <fstream>
#include <iostream>

using namespace llvm;
using namespace clang;
using namespace clang::tooling;

//重写的静态对象
static clang::Rewriter TheRewriter;

//filePath
// static std::string filePath;

// 插入的内容
static std::string statsStmt("printf(\"文件名:%s, 方法名:%s, 行数:%d, 时间:%s,\\n\", [NSString stringWithUTF8String:__FILE__].lastPathComponent.UTF8String,__PRETTY_FUNCTION__,__LINE__,__TIME__);");

//OptionCategory指定了工具所处的分类随意就好
static cl::OptionCategory OptsCategory("ClangAutoStats");

namespace ClangAutoStats {// namespace start

//断点调试代码
void breakPoint (std::string str,bool bool1) {
    outs() << str << "\n";
    if (!bool1) {
        return;
    }
    int i = 8;
    assert(i < 0 && "有效");
}

#pragma mark RecursiveASTVisitor
/*
 RecursiveASTVisitor 是一个深度优先遍历 AST 和访问节点的类
 1遍历（Traverse） AST 的每个节点；
 2回溯（WalkUp） 在某一个节点，访问这个节点的层次结构 ( 每个节点也是一个树 )；
 3访问（Visit）如果某一个节点是一种类型的动态类别 ( 比如是一个子类等 )，调用一个用户重载的函数来访问这个节点；
 */
class ClangAutoStatsVisitor: public RecursiveASTVisitor<ClangAutoStatsVisitor> {
    
private:
    // used for getting additional AST info
    ASTContext *astContext;
    
    //typedef clang::RecursiveASTVisitor<RewritingVisitor> Base;
    Rewriter &rewriter;
    
    CompilerInstance *CI;
    
public:
    explicit ClangAutoStatsVisitor(Rewriter &R)
       : rewriter{R} // initialize private members
       {}
    
    explicit ClangAutoStatsVisitor(ASTContext *Ctx,Rewriter &R,CompilerInstance *aCI) :
    astContext(Ctx),rewriter{R},CI(aCI) {}
    
    bool VisitObjCImplementationDecl(ObjCImplementationDecl *ID) {

        outs() << "VisitObjCImplementationDecl start" << "\n";

        // 遍历 decls
        for (auto D : ID->decls()) {
            if (ObjCMethodDecl *MD = dyn_cast<ObjCMethodDecl>(D)) {
                handleObjcMethDecl(MD);
            }
        }

        outs() << "VisitObjCImplementationDecl completed" << "\n";

        return true;
    }
    
//    bool VisitPragmaCommentDecl(PragmaCommentDecl *ID) {
//
//        outs() << "VisitPragmaCommentDecl start" << "\n";
//
//        outs() << "VisitPragmaCommentDecl completed" << "\n";
//        return true;
//    }
//    
//    bool VisitPragmaDetectMismatchDecl(PragmaDetectMismatchDecl *ID) {
//        outs() << "VisitPragmaCommentDecl start" << "\n";
//
//        outs() << "VisitPragmaCommentDecl completed" << "\n";
//        return true;
//    }
    
    // 处理方法声明
    bool handleObjcMethDecl(ObjCMethodDecl *MD) {
        
        if (!MD->hasBody()) return true;

        outs() << "decl name: " << MD->getNameAsString() << "\n";

        // 复合语句stmt
        CompoundStmt *cmpdStmt = MD->getCompoundBody();
        // 获取指向大括号后的位置
        SourceLocation loc = cmpdStmt->getLBracLoc().getLocWithOffset(1);

        // 如果是宏定义展开的话，需要特殊处理
        if (loc.isMacroID()) {
            // loc = rewriter.getSourceMgr().getImmediateExpansionRange(loc).first;
            outs() << "deal with MarcroId" << "n";
            loc = rewriter.getSourceMgr().getImmediateExpansionRange(loc).getBegin();
        }

        std::string funcName = MD->getDeclName().getAsString();

        // 方法body
        Stmt *methodBody = MD->getBody();

        // 源码
        std::string srcCode;
        // 获取源码片段
        srcCode.assign(astContext->getSourceManager().getCharacterData(methodBody->getSourceRange().getBegin()),methodBody->getSourceRange().getEnd().getRawEncoding()-methodBody->getSourceRange().getBegin().getRawEncoding()+1);

        // 设置要插入的语句
        std::string annotateStmt(statsStmt);
        
        // 获取 __attribute__((annotate("1234")))
        AnnotateAttr *annotateAttr = MD->getAttr<AnnotateAttr>();
        if (annotateAttr != NULL) {
            // 获取 annotate 内容
            std::string annoateContent(annotateAttr->getAnnotation());
            // 替换 annotateStmt
            std::string replaceTarget("printf(\"");
            annotateStmt.replace(0, replaceTarget.length(), replaceTarget + annoateContent);
        }
        
        // 判断是否已经打过点，打过点的进行忽略
        std::string codes(srcCode);
        size_t pos = 0;
        bool isTaged = false;
        while ((pos = codes.find(annotateStmt, pos)) != std::string::npos) {
           codes.replace(pos, annotateStmt.length(), funcName);
           pos += funcName.length();
           isTaged = true;
        }
        
        outs() << "******** decl body start ********" << "\n";
        outs() << srcCode << "\n";
        outs() << "******** decl body end ********" << "\n";

        if (isTaged) {
            return true;
        }

        if (rewriter.isRewritable(loc)) {
            errs() << "file is not rewritable" <<  "\n";
        }else{
            outs() << "file is rewritable" <<  "\n";
        }

        // 插入代码
        TheRewriter.InsertTextBefore(loc, annotateStmt);
        outs() << "insert stats code success" << "\n";

        return true;
    }
};

#pragma mark ASTConsumer

/**
 HandleTopLevelDecl是在遍历到Decl（即声明或定义，例如函数、ObjC interface等）的时候立即回调，而HandleTranslationUnit则是在当前的TranslationUnit（即目标文件或源代码）的AST被完整解析出来后才会回调。
 TopLevel指的是在AST第一层的节点，对于OC代码来说，这一般是interface、implementation、全局变量等在代码最外层的声明或定义
 */
class ClangAutoStatsASTConsumer : public ASTConsumer {
    
private:
    // AST 遍历器
    ClangAutoStatsVisitor Visitor;
    // 编译器实例
    CompilerInstance *CI;
    
public:
    // override the constructor in order to pass CI
    explicit ClangAutoStatsASTConsumer(Rewriter &R)
       : Visitor(R) // initialize the visitor
       {}
    
    explicit ClangAutoStatsASTConsumer(CompilerInstance *aCI,Rewriter &R)
    : Visitor(&(aCI->getASTContext()), R, aCI), CI(aCI) {}
    
    // 这是 ClangAutoStatsASTConsumer 的入口。当整个抽象语法树 (AST) 构造完成以后，HandleTranslationUnit 这个函数将会被 Clang 的驱动程序调用 replace未调用
    void HandleTranslationUnit(ASTContext &context) override {
        
        TranslationUnitDecl *decl = context.getTranslationUnitDecl();
        Visitor.TraverseTranslationUnitDecl(decl);
    }
};


#pragma mark PluginASTAction

// ASTFrontendAction是用来为前端工具定义标准化的AST操作流程的。一个前端可以注册多个Action，然后在指定时刻轮询调用每一个Action的特定方法
class ClangAutoStatsAction : public PluginASTAction {
    
private:
    Rewriter rewriter;
    
public:
    // 返回自己创建并返回给前端一个ASTConsumer
    virtual std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI, StringRef file) override {
        
        rewriter.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());
        // 获取当前预处理器
        CI.getPreprocessor();
        
        // 获取原文件路径
        std::string fileName(file.data());
        
        // 设置 rewriter
        TheRewriter.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());
        
        // 返回 ASTConsumer 实例
        outs() << "create CreateASTConsumer success for " << "("+fileName+")" << "\n";
        return std::make_unique<ClangAutoStatsASTConsumer>(&CI, rewriter);
    }
    
    bool ParseArgs(const CompilerInstance &CI, const std::vector<std::string>& args) override{
       return true;
    }

    bool PrepareToExecuteAction(CompilerInstance &CI) override{
        return true;
    }

    bool BeginInvocation(CompilerInstance &CI) override {
         return true;
    }

    
//    PluginASTAction::ActionType getActionType() override {
//       return ReplaceAction;
//    }
//
//    bool BeginSourceFile(CompilerInstance &CI, const FrontendInputFile &Input) {
//        breakPoint("BeginSourceFile 执行", true);
//        return true;
//    }
//     bool BeginSourceFileAction(CompilerInstance &CI) override {
//         breakPoint("BeginSourceFileAction 执行", true);
//         return true;
//    }
//
//    void ExecuteAction() override{
//         outs() << "action 处理 .... success" << "\n";
//    }
    
    // 每个文件在parse完之后，做一些清理和内存释放工作 -obj ParseSyntaxOnly
    void EndSourceFileAction() override {

        // 获取源码管理器
        SourceManager &SM = TheRewriter.getSourceMgr();
        
        // 原文件路径
        std::string sourceFilePath = SM.getFileEntryForID(SM.getMainFileID())->getName().str();
        // 重写后的文件路径
        std::string rewritedFilePath;
        // 获取 . 符号的位置
        size_t pos = sourceFilePath.find_last_of(".");

        if (pos != std::string::npos) {
            // 获取重写后的文件路径
            rewritedFilePath = sourceFilePath.substr(0,pos) +"_rewrited" + sourceFilePath.substr(pos, sourceFilePath.length());
        }

        std::error_code error_code;
        llvm::raw_fd_ostream outFile(rewritedFilePath, error_code, llvm::sys::fs::F_None);
        // 将Rewriter结果输出到文件中
        TheRewriter.getEditBuffer(SM.getMainFileID()).write(outFile);
    }
};

} // namespace end

// 注册插件
static clang::FrontendPluginRegistry::Add
<ClangAutoStats::ClangAutoStatsAction>Z("ClangPluginAutoStats","ClangPluginAutoStats plugin for auto stat");

/*自动打点方案C.L.A.S.缺点
 无法适用于条件打点---条件打点为动态打点，本方案为静态打点适合80%场景
 插入的代码可能会造成编译失败---引入了其他文件这个可以通过配置CLAS插入用户指定的#include或#import
 插入范围过大---优化设计
 编译出的文件包含与源文件不符的Debug信息---生成的DebugSymbols是与临时文件(.clas.m)的信息相符的，与源文件并不相符，这个就需要我们在生成dSYMs的时候，把所有的临时文件信息替换为原始文件信息，为了达到这个目的，我们需要修改LLVM的dsymutil替换系统原生的dsymutil
 插入代码导致二进制体积变大---一到两条语句为佳，避免在插入代码里直接构造含有复杂逻辑和功能的语句。{ [MCStatistik logEvent:@"%__FUNC_NAME__%"]; }
 */

/*
 CLAS执行完成后，还有一个非常重要的任务，就是将原文件.m重命名后，将CLAS输出的临时文件重命名为原文件，拼接剩余参数并调用苹果原生的Clang（/usr/bin/clang），clang执行完成后，无论成功与否，将临时文件删除并将原文件.m复原，编译流程至此结束。
 
 1、集成Xcode编译链中
 2、编译出的文件包含与源文件不符的Debug信息
 后续参考pass BugpointPasses
 */

#pragma mark Main

// 函数入口
int main(int argc, const char **argv) {
    
    CommonOptionsParser op(argc, argv, OptsCategory);
    ClangTool Tool(op.getCompilations(), op.getSourcePathList());
    int result = Tool.run(newFrontendActionFactory<ClangAutoStats::ClangAutoStatsAction>().get());
    return result;
}
