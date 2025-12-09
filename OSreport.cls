% xjtuosreport.cls
% 西安交通大学操作系统专题实验报告模板类文件

\NeedsTeXFormat{LaTeX2e}
\ProvidesClass{OSreport}[2023/10/27 v1.1 XJTU OS Experiment Report]

% ==================================================
% 修复部分：显式指定 fontset=fandol 以解决 Mac 报错
% 如果您希望使用 Mac 系统自带字体，可改为 fontset=macnew
% ==================================================
\LoadClass[a4paper,12pt,fontset=macnew]{ctexart}

% 导入必要的宏包
\RequirePackage{geometry}   % 页面设置
\RequirePackage{graphicx}   % 图片支持
\RequirePackage{fancyhdr}   % 页眉页脚
\RequirePackage{titletoc}   % 目录设置
\RequirePackage{amsmath}    % 数学公式
\RequirePackage{setspace}   % 行距设置
\RequirePackage{ulem}       % 下划线
\RequirePackage{hyperref}   % 超链接和书签

% 页面边距设置
\geometry{
    left=2.5cm,
    right=2.5cm,
    top=2.5cm,
    bottom=2.5cm
}

% 定义个人信息变量
\newcommand{\@stclass}{}
\newcommand{\@stuid}{}
\newcommand{\@stname}{}
\newcommand{\@stdate}{\today}

% 提供外部接口命令
\newcommand{\stclass}[1]{\renewcommand{\@stclass}{#1}}
\newcommand{\stuid}[1]{\renewcommand{\@stuid}{#1}}
\newcommand{\stname}[1]{\renewcommand{\@stname}{#1}}
\newcommand{\stdate}[1]{\renewcommand{\@stdate}{#1}}

% 设置章节编号样式
% 一级标题：一、
% 二级标题：（一）
% 三级标题：1．
\ctexset{
    section = {
        format = \Large\bfseries\centering,
        number = \chinese{section},
        aftername = {},
        indent = 0pt
    },
    subsection = {
        format = \large\bfseries\raggedright,
        number = （\chinese{subsection}）,
        indent = 0pt
    },
    subsubsection = {
        format = \normalsize\bfseries,
        number = \arabic{subsubsection}．,
        indent = 2em
    }
}

% 封面设计
\newcommand{\makecover}{
    \begin{titlepage}
        \centering
        \vspace*{2cm}
        
        % 学校名称
        {\Huge \bfseries 西安交通大学 \par}
        
        \vspace{3cm}
        
        % 报告标题
        {\Huge \bfseries 操作系统专题实验报告 \par}
        
        \vspace{5cm}
        
        % 学生信息表格
        {\Large
            \renewcommand{\arraystretch}{1.5}
            \begin{tabular}{rc}
                \bfseries 班级： & \underline{\makebox[6cm][c]{\@stclass}} \\
                \bfseries 学号： & \underline{\makebox[6cm][c]{\@stuid}} \\
                \bfseries 姓名： & \underline{\makebox[6cm][c]{\@stname}} \\
            \end{tabular}
        \par}
        
        \vfill
        
        % 日期
        {\Large \@stdate \par}
        
        \vspace{2cm}
    \end{titlepage}
}

% 页眉页脚设置
\pagestyle{fancy}
\fancyhf{}
\fancyhead[C]{西安交通大学操作系统专题实验报告}
\fancyfoot[C]{\thepage}

% 链接颜色设置
\hypersetup{
    colorlinks=true,
    linkcolor=black,
    filecolor=black,
    urlcolor=blue,
    citecolor=black
}

% 设置行距
\onehalfspacing

\endinput