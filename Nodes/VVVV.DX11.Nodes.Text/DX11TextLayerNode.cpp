#include "StdAfx.h"
#include "DX11TextLayerNode.h"

#include "FontWrapperFactory.h"

using namespace FeralTic::Utils;

namespace VVVV { namespace Nodes { namespace DX11 {

DX11TextLayerNode::DX11TextLayerNode(IIOFactory^ factory, SlimDX::DirectWrite::Factory^ dwFactory)
{
	this->dwFactory = dwFactory;
	this->iofactory = factory;
	factory->PluginHost->CreateTransformInput("Transform In",TSliceMode::Dynamic,TPinVisibility::True,this->FInTr);
	this->FInTr->Order = 1;
	this->fontrenderers = gcnew	Dictionary<DX11RenderContext^,IntPtr>();

	InputAttribute^ colorAttribute = gcnew InputAttribute("Color");
	colorAttribute->Order = 6;
	colorAttribute->DefaultColor = MagicNumberUtils::WhiteDefault();

	this-> FInColor = IOFactoryExtensions::CreateSpread<SlimDX::Color4>(this->iofactory, colorAttribute, true);
}

void DX11TextLayerNode::Evaluate(int SpreadMax)
{
	this->spmax = SpreadMax;

	if (this->FOutLayer[0] == nullptr)
    {
		this->FOutLayer[0] = gcnew DX11Resource<DX11Layer^>();
	}
}

void DX11TextLayerNode::Update(DX11RenderContext^ context)
{
    if (!this->FOutLayer[0]->Contains(context))
    {
		this->FOutLayer[0][context] = gcnew DX11Layer();
        this->FOutLayer[0][context]->Render = gcnew RenderDelegate<DX11RenderSettings^>(this,&DX11TextLayerNode::Render);
    }

	if (!this->FTextRenderer->IsConnected)
	{
		if (!this->fontrenderers->ContainsKey(context))
		{
			IFW1FontWrapper* pw = FontWrapperFactory::GetWrapper(context, this->dwFactory);
			this->fontrenderers->Add(context, IntPtr(pw));
		}
	}
}


void DX11TextLayerNode::Destroy(DX11RenderContext^ context, bool force)
{
    if (this->FOutLayer[0]->Contains(context))
    {
		this->FOutLayer[0]->Dispose(context);
	}
}

void DX11TextLayerNode::Render(DX11RenderContext^ context, DX11RenderSettings^ settings)
{
	if (this->FInEnabled[0])
	{
		float w = (float)settings->RenderWidth;
		float h = (float)settings->RenderHeight;

		IFW1FontWrapper* fw = 0;
		if (this->FTextRenderer->IsConnected)
		{
			fw = (IFW1FontWrapper*)FTextRenderer[0][context]->NativePointer().ToPointer();
		}
		else
		{
			fw  = (IFW1FontWrapper*)this->fontrenderers[context].ToPointer();
		}


		ID3D11Device* dev = (ID3D11Device*)context->Device->ComPointer.ToPointer();
		ID3D11DeviceContext* pContext = (ID3D11DeviceContext*)context->CurrentDeviceContext->ComPointer.ToPointer();

		IFW1GlyphRenderStates* pRenderStates;
		fw->GetRenderStates(&pRenderStates);

		int cnt;
		float* tr;
		this->FInTr->GetMatrixPointer(cnt,tr);

		SlimDX::Matrix* smp = (SlimDX::Matrix*)&tr[0];

		bool applyState = this->FStateIn->IsConnected;

		for (int i = 0; i < this->spmax; i++)
		{

			void* txt = (void*)Marshal::StringToHGlobalUni(this->FInString[i]);
			void* font = (void*)Marshal::StringToHGlobalUni(this->FFontInput[i]);
			float size = this->FInSize[i];
			SlimDX::Color4 c = this->FInColor[i];
			c.Red = this->FInColor[i].Blue;
			c.Blue = this->FInColor[i].Red;
			unsigned int color = c.ToArgb();
			

			SlimDX::Matrix preScale = SlimDX::Matrix::Scaling(1.0f,-1.0f,1.0f);
			switch (this->FNormalizeInput[i]->Index)
			{
			case 1: preScale = SlimDX::Matrix::Scaling(1.0f / w, -1.0f / w, 1.0f); break;
			case 2: preScale = SlimDX::Matrix::Scaling(1.0f / h, -1.0f / h, 1.0f); break;
			case 3: preScale = SlimDX::Matrix::Scaling(1.0f / w, -1.0f / h, 1.0f); break;
			}

			SlimDX::Matrix sm = smp[i % this->FInTr->SliceCount];

			SlimDX::Matrix mat = SlimDX::Matrix::Multiply(preScale,sm);
			mat = SlimDX::Matrix::Multiply(mat,settings->View);
			mat = SlimDX::Matrix::Multiply(mat,settings->Projection);

			float* tr = (float*)&mat;

			FW1_RECTF rect = {0.0f, 0.0f, 0.0f, 0.0f};

			int flag = FW1_NOWORDWRAP;

			if (this->FHorizontalAlignInput[i]->Index == 0) { flag |= FW1_LEFT; }
			else if (this->FHorizontalAlignInput[i]->Index == 1) { flag |= FW1_CENTER; }
			else if (this->FHorizontalAlignInput[i]->Index == 2) { flag |= FW1_RIGHT; }

			if (this->FVerticalAlignInput[i]->Index == 0) { flag |= FW1_TOP; }
			else if (this->FVerticalAlignInput[i]->Index == 1) { flag |= FW1_VCENTER; }
			else if (this->FVerticalAlignInput[i]->Index == 2) { flag |= FW1_BOTTOM; }


			if (applyState)
			{
				pRenderStates->SetStates(pContext, 0);

				context->RenderStateStack->Push(this->FStateIn[i]);

				fw->DrawString(
					pContext,
					(WCHAR*)txt,
					(WCHAR*)font,
					size,
					&rect,
					color,
					NULL,
					tr,
					flag | FW1_STATEPREPARED
					);

				context->RenderStateStack->Pop();
			}
			else
			{
				fw->DrawString(
					pContext,
					(WCHAR*)txt,
					(WCHAR*)font,
					size,
					&rect,
					color,
					NULL,
					tr,
					flag
					);
			}



			Marshal::FreeHGlobal(System::IntPtr(txt));
			Marshal::FreeHGlobal(System::IntPtr(font));
		}

		//Apply old states back
		context->RenderStateStack->Apply();
		context->CleanShaderStages();
	}
}

}}}