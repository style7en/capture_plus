namespace CapturePlus.Core;

public static class PromptBuilder
{
    public static string AiAnalysis()
        => "请分析这张截图的内容，说明图中展示了什么信息，并提炼关键要点。";

    public static string Translate(string targetLanguage, string text)
        => $"将以下文本翻译为{targetLanguage}，只输出译文，保留原格式：\n\n{text}";

    public static string AiOcr()
        => "请提取并输出这张图片中的所有文字内容，保留原文的换行和排版结构，只输出提取的文字，不要添加解释。";
}
