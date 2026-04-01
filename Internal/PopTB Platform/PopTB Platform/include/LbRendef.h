

class RENDERCLASS : public RenderBase
{
public:
    virtual void LbDraw_Pixel(SINT x, SINT y, TbColour colour);

    virtual void LbDraw_Rectangle(const Pop3Rect &rect, TbColour col);
    virtual void LbDraw_RectangleOutline(const Pop3Rect &rect, TbColour col);
    virtual void LbDraw_RectangleFilled(const Pop3Rect &rect, TbColour colour);

    virtual void LbDraw_Circle(SINT x, SINT y, UINT radius, TbColour col);
    virtual void LbDraw_CircleOutline(SINT xpos, SINT ypos, UINT radius, TbColour colour);
    virtual void LbDraw_CircleFilled(SINT xpos, SINT ypos, UINT radius, TbColour colour);

    virtual void LbDraw_Line(SINT x1, SINT y1, SINT x2, SINT y2, TbColour colour);
    virtual void LbDraw_HorizontalLine(SINT x, SINT y, SINT length, TbColour colour);
    virtual void LbDraw_VerticalLine(SINT x, SINT y, SINT height, TbColour colour);

    virtual void LbDraw_Triangle(SINT x1, SINT y1, SINT x2, SINT y2, SINT x3, SINT y3, TbColour colour);

    virtual void LbDraw_ClearClipWindow(TbColour colour);

    virtual void LbDraw_PropText(SINT x, SINT y, const TBCHAR *pText, TbColour colour);
    virtual void LbDraw_UnicodePropText(SINT x, SINT y, const UNICHAR *pText, TbColour colour);

    //virtual	void BfDraw_Sprite(SINT x, SINT y, const TbSprite *lpSprite);
   // virtual	void BfDraw_OneColourSprite(SINT x, SINT y, const TbSprite* lpSprite, TbColour colour);
};


